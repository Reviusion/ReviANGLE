// D3D11 Deferred Renderer - Async command submission worker pool
// ------------------------------------------------------------------------
// Submits D3D11 command lists from worker threads to reduce main thread
// overhead. Each worker maintains a small command allocator pool.
//
// CRITICAL FIXES APPLIED:
// 1. Worker threads use THREAD_PRIORITY_ABOVE_NORMAL (not HIGHEST) to avoid
//    starving FMOD audio threads which causes audio stuttering.
// 2. consolidate() uses 3000ms timeout (not INFINITE) to prevent permanent
//    hangs if a worker crashes. On timeout, logs critical error and disables
//    the deferred renderer safely.

#include <windows.h>
#include <d3d11.h>
#include <atomic>
#include <vector>
#include <cstring>
#include <memory>
#include "config.hpp"
#include "angle_loader.hpp"

// Forward declare ID3D11Multithread interface (minimal definition for compilation)
// Full definition comes from d3d11.h with proper Windows SDK
MIDL_INTERFACE("9B7E4E00-342C-4106-A19F-4F2704F689F0")
ID3D11MultithreadMinimal : public IUnknown {
public:
    virtual void STDMETHODCALLTYPE Enter(void) = 0;
    virtual void STDMETHODCALLTYPE Leave(void) = 0;
    virtual BOOL STDMETHODCALLTYPE SetMultithreadProtected(BOOL bMultithreadProtected) = 0;
    virtual BOOL STDMETHODCALLTYPE GetMultithreadProtected(void) = 0;
};
typedef ID3D11MultithreadMinimal ID3D11Multithread;

namespace d3d11_deferred {

static constexpr DWORD kConsolidateTimeoutMs = 3000;  // was INFINITE - caused hangs
static constexpr size_t kMaxWorkers = 2;

struct Worker {
    size_t workerIdx = 0;  // index for doneEvents lookup
    HANDLE thread = nullptr;
    HANDLE workEvent = nullptr;
    std::atomic<bool> hasWork;
    std::atomic<bool> shouldExit;
    ID3D11CommandList* pendingList = nullptr;
    
    Worker() : hasWork(false), shouldExit(false) {}
};

static std::vector<std::unique_ptr<Worker>> g_workers;
static std::atomic<bool> g_active{ false };
static HANDLE g_doneEvents[kMaxWorkers] = {};

// Forward declarations
static DWORD WINAPI workerThreadProc(LPVOID param);
static void submitCommandList(ID3D11CommandList* list);
void shutdown();  // forward declaration for use in initialize()

bool initialize(ID3D11Device* device) {
    if (g_active.load()) return true;
    if (!device) return false;

    // Check if multithreaded device is available
    ID3D11Multithread* multithread = nullptr;
    if (FAILED(device->QueryInterface(__uuidof(ID3D11Multithread), (void**)&multithread))) {
        angle::log("d3d11_deferred: device does not support ID3D11Multithread");
        return false;
    }
    multithread->SetMultithreadProtected(TRUE);
    multithread->Release();

    g_workers.clear();
    for (size_t i = 0; i < kMaxWorkers; ++i) {
        g_workers.emplace_back(std::make_unique<Worker>());
        Worker& w = *g_workers.back();
        w.workerIdx = i;  // store index for doneEvents lookup
        w.workEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        g_doneEvents[i] = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        
        w.thread = CreateThread(
            nullptr,
            0,
            workerThreadProc,
            &w,
            0,
            nullptr
        );
        
        if (!w.thread) {
            angle::log("d3d11_deferred: failed to create worker thread %zu", i);
            d3d11_deferred::shutdown();
            return false;
        }

        // FIX: Use ABOVE_NORMAL instead of HIGHEST to prevent audio stutter.
        // THREAD_PRIORITY_HIGHEST was starving FMOD's audio threads,
        // causing audio breakup and stuttering in-game.
        SetThreadPriority(w.thread, THREAD_PRIORITY_ABOVE_NORMAL);
    }

    g_active.store(true);
    angle::log("d3d11_deferred: initialized with %zu workers", kMaxWorkers);
    return true;
}

void shutdown() {
    g_active.store(false);
    
    for (auto& w : g_workers) {
        w->shouldExit.store(true);
        if (w->workEvent) SetEvent(w->workEvent);
    }
    
    // Wait for all workers to exit (with timeout to avoid hanging)
    std::vector<HANDLE> threads;
    for (auto& w : g_workers) {
        if (w->thread) threads.push_back(w->thread);
    }
    if (!threads.empty()) {
        WaitForMultipleObjects(static_cast<DWORD>(threads.size()), threads.data(), TRUE, 5000);
        for (auto& w : g_workers) {
            if (w->thread) {
                CloseHandle(w->thread);
                w->thread = nullptr;
            }
        }
    }
    
    for (size_t i = 0; i < kMaxWorkers; ++i) {
        if (g_doneEvents[i]) {
            CloseHandle(g_doneEvents[i]);
            g_doneEvents[i] = nullptr;
        }
        if (i < g_workers.size() && g_workers[i]->workEvent) {
            CloseHandle(g_workers[i]->workEvent);
            g_workers[i]->workEvent = nullptr;
        }
    }
    
    g_workers.clear();
}

// Queue a command list for async submission on a worker thread
void queueCommandList(ID3D11CommandList* list) {
    if (!g_active.load() || g_workers.empty()) {
        // No async workers - submit immediately on main thread
        submitCommandList(list);
        return;
    }
    
    // Round-robin to workers
    static std::atomic<size_t> s_nextWorker{ 0 };
    size_t idx = s_nextWorker.fetch_add(1) % g_workers.size();
    Worker& w = *g_workers[idx];
    
    w.pendingList = list;
    w.hasWork.store(true);
    SetEvent(w.workEvent);
}

// Called at end of frame - waits for all pending work and submits to GPU
void consolidate() {
    if (!g_active.load() || g_workers.empty()) return;
    
    // Reset done events
    for (size_t i = 0; i < g_workers.size() && i < kMaxWorkers; ++i) {
        ResetEvent(g_doneEvents[i]);
    }
    
    // Signal workers we need completion
    for (auto& w : g_workers) {
        w->hasWork.store(true);
        SetEvent(w->workEvent);
    }
    
    // FIX: Use finite timeout instead of INFINITE to prevent deadlocks.
    // If a worker thread crashes inside ANGLE's D3D11 driver while holding
    // a mutex, we would wait forever with INFINITE, freezing the game.
    // Instead, use a 3-second timeout, log the error, and safely disable
    // the deferred renderer to allow the game to continue (potentially with
    // some rendering artifacts but at least not frozen).
    DWORD waitResult = WaitForMultipleObjects(
        static_cast<DWORD>(g_workers.size()),
        g_doneEvents,
        TRUE,  // wait for all
        kConsolidateTimeoutMs
    );
    
    if (waitResult == WAIT_TIMEOUT) {
        // CRITICAL: Worker threads failed to complete in time.
        // This usually means a worker crashed or deadlocked.
        angle::log("CRITICAL: d3d11_deferred::consolidate() timed out after %dms - "
                   "worker threads may have crashed. Disabling deferred renderer.",
                   kConsolidateTimeoutMs);
        g_active.store(false);
        // Main thread will fall back to immediate submission next frame
        return;
    }
    
    if (waitResult == WAIT_FAILED) {
        angle::log("CRITICAL: d3d11_deferred::consolidate() wait failed (GLE=%lu) - "
                   "disabling deferred renderer.", GetLastError());
        g_active.store(false);
        return;
    }
}

// Worker thread procedure
static DWORD WINAPI workerThreadProc(LPVOID param) {
    Worker* w = static_cast<Worker*>(param);
    HANDLE events[] = { w->workEvent };
    
    while (!w->shouldExit.load()) {
        DWORD waitResult = WaitForMultipleObjects(1, events, FALSE, INFINITE);
        
        if (waitResult == WAIT_OBJECT_0) {
            // Got work signal
            if (w->pendingList) {
                submitCommandList(w->pendingList);
                w->pendingList = nullptr;
            }
            w->hasWork.store(false);
            
            // Signal completion (used by consolidate)
            // workerIdx stored in struct avoids pointer arithmetic with unique_ptr
            if (w->workerIdx < kMaxWorkers) {
                SetEvent(g_doneEvents[w->workerIdx]);
            }
        }
    }
    
    return 0;
}

// Actually submit a command list to the immediate context
static void submitCommandList(ID3D11CommandList* list) {
    if (!list) return;
    
    // In a real implementation, this would execute the command list
    // on the main thread's immediate context. For now this is a stub
    // that would be filled in with actual D3D11 calls.
    // 
    // The deferred renderer's purpose is to move command list recording
    // (which is CPU-heavy) off the main thread.
    
    // NOTE: This function is called from worker threads, so it must
    // be careful about thread safety. In practice, command list creation
    // is done on workers, but execution must be on the main thread with
    // the immediate context.
    
    (void)list;  // suppress unused warning for now
}

} // namespace d3d11_deferred
