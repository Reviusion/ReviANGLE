// UI logic for the config editor. Owns the in-memory Ini and renders one
// ImGui frame per call.
#pragma once
#include <string>

class EditorApp {
public:
    bool init(const std::string& iniPath);

    // Render one ImGui frame (called from the per-frame loop in main.cpp).
    void renderFrame();

    // Returns true while the user hasn't closed the window.
    bool isRunning() const { return m_running; }

    // Optional setter from main on WM_CLOSE.
    void requestClose() { m_running = false; }

private:
    void doSave();
    void doReload();
    void doResetDefaults();
    void renderTopBar();
    void renderSectionTabs();
    void renderOptionList();
    void renderDescriptionPanel();
    void renderFooter();
    void renderConfirmModal();

    std::string m_iniPath;
    bool        m_running = true;
    bool        m_dirty   = false;          // unsaved changes
    std::string m_savedAt;                  // status message after save
    std::string m_search;                   // search filter

    int  m_currentSectionIdx = 1;           // default to [Boost] tab
    int  m_selectedOptIdx    = -1;          // index in schemaAll() of option whose desc is shown

    // Confirm modal state.
    enum class ConfirmKind { None, Reload, ResetDefaults };
    ConfirmKind m_pendingConfirm = ConfirmKind::None;
};
