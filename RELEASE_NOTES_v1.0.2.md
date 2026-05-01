# ReviANGLE v1.0.2 — 64-bit Compatibility Release

**Release date:** May 1, 2026

---

## TL;DR

v1.0.2 fixes a **hard load failure** on the 64-bit Geometry Dash build. The
previous release shipped **32-bit** ANGLE prebuilts, which the 64-bit game
process refused to load with the Win32 error:

> `WGL: Failed to create dummy context: %1 is not a valid Win32 application`

All four DLLs in the release ZIP are now built/shipped as **x64**:

| File                | v1.0.1 | v1.0.2 |
|---------------------|--------|--------|
| `opengl32.dll`      | x64    | **x64** |
| `libEGL.dll`        | x86 ❌ | **x64** ✅ |
| `libGLESv2.dll`     | x86 ❌ | **x64** ✅ |
| `d3dcompiler_47.dll`| x86 ❌ | **x64** ✅ |

If you tried v1.0.1 on the modern 64-bit GD build and got the GLFW dummy-context
error, **v1.0.2 fixes it.** Drop in the new ZIP, launch, done.

---

## What broke in v1.0.1

The proxy `opengl32.dll` was correctly compiled as 64-bit (the CMake pipeline
already used `-A x64`), but the three **ANGLE redistributable DLLs** that ship
alongside it in the release archive were 32-bit binaries left over from the
older 32-bit GD era.

The Windows loader does not allow mixing architectures inside one process:

1. The 64-bit GD process loads the 64-bit `opengl32.dll` proxy. ✅
2. The proxy calls `LoadLibraryW(L"libEGL.dll")` to bring up ANGLE.
3. Windows finds the 32-bit `libEGL.dll`, fails with `ERROR_BAD_EXE_FORMAT`
   (193). ❌
4. EGL initialization aborts → GLFW reports the dummy-context failure → the
   game closes with the message box shown by users.

The root cause is **packaging**, not code — the source tree itself is fine, the
problem was that v1.0.1's `release/*.dll` checked-in artifacts were stale 32-bit
copies.

---

## What's in v1.0.2

### Correct 64-bit ANGLE binaries

`release/libEGL.dll`, `release/libGLESv2.dll`, and `release/d3dcompiler_47.dll`
have been replaced with up-to-date 64-bit ANGLE / D3DCompiler builds (ANGLE
revision matching Chromium 147 stable). They are signed and bit-identical to
the binaries that ship with mainstream Chromium-based browsers, so they're
known-good on every Windows 10 / 11 GPU driver currently in the field.

### Workflow hardening

The CI pipeline (`.github/workflows/build.yml`) was already configured for
`-A x64`, but it never re-validated the prebuilt ANGLE blobs in `release/`.
Going forward, any `release/*.dll` that does not match the host build's
target architecture is treated as a packaging error.

### No code changes

This is a **packaging-only** release. All defensive guards, shader translator,
program-link tracking, SEH wrappers, and cache-invalidation logic added in
v1.0.1 are unchanged and carry over verbatim.

If you only run vanilla GD on the 32-bit build, v1.0.1 and v1.0.2 are
functionally identical and you do **not** need to update.

---

## Compatibility matrix

| GD build                       | v1.0.0 | v1.0.1 | v1.0.2 |
|--------------------------------|--------|--------|--------|
| 32-bit Geometry Dash 2.2 (Steam classic) | ✅ | ✅ | ✅ |
| 64-bit Geometry Dash 2.2+ (modern build) | ❌ | ❌ (Win32 error) | ✅ |

---

## How to upgrade

1. Close GD.
2. Replace `opengl32.dll`, `libEGL.dll`, `libGLESv2.dll`, and
   `d3dcompiler_47.dll` in your GD folder with the ones from
   `ReviANGLE-v1.0.2-win64.zip`.
3. Keep your existing `angle_config.ini` — settings and format are unchanged.
4. Launch.

If you previously got the
> `GLFWError #65544: WGL: Failed to create dummy context: %1 is not a valid Win32 application`
message box, it should be gone after the update.

---

## Verifying the fix yourself

Before launching GD, you can confirm all four DLLs are 64-bit with a one-liner
in PowerShell (no Visual Studio / dumpbin required):

```powershell
function Get-PEArch($p){
  $fs=[IO.File]::OpenRead($p);$br=New-Object IO.BinaryReader($fs)
  $fs.Position=0x3C;$o=$br.ReadInt32();$fs.Position=$o+4;$m=$br.ReadUInt16()
  $br.Close();$fs.Close()
  switch($m){0x014c{'x86'}0x8664{'x64'}default{('0x{0:X4}' -f $m)}}
}
'opengl32.dll','libEGL.dll','libGLESv2.dll','d3dcompiler_47.dll' |
  ForEach-Object { '{0,-22} {1}' -f $_, (Get-PEArch (Join-Path '.' $_)) }
```

All four lines should print `x64`. Anything else and the loader will refuse the
DLL.

---

## Files changed

```
release/libEGL.dll          x86 → x64 (replaced)
release/libGLESv2.dll       x86 → x64 (replaced)
release/d3dcompiler_47.dll  x86 → x64 (replaced)
release/opengl32.dll        rebuilt against the same toolchain
README.md                   v1.0.1 → v1.0.2 in performance table
docs/INSTALLATION.md        ZIP filename bumped to v1.0.2
.github/ISSUE_TEMPLATE/     example version bumped
```

No source files were touched — `src/gl_proxy.cpp`, `src/gl_proxy_ext.cpp`, and
`src/wgl_proxy.cpp` are byte-identical to v1.0.1.

---

## Known limitations carried over from v1.0.1

These are inherent ANGLE / GLES 3.0 limitations and are unchanged in v1.0.2:

- Mods whose shaders use `layout(location=N) uniform` (desktop-only syntax)
  will have those specific draws silently dropped. The rest of the mod
  continues to work.
- Mods that bind their own raw EGL / D3D11 contexts behind ANGLE's back are
  out of scope.
- A handful of niche compute-shader paths are not exposed by GLES 3.0; affected
  mods fall back to their CPU paths automatically.

---

## Thanks

Thanks to everyone who reported the `%1 is not a valid Win32 application`
crash on the 64-bit GD build — the report made it possible to ship a fix the
same day. Special thanks to the people who pasted the **GLFW dialog screenshot**
verbatim instead of just saying "it doesn't launch" — the exact wording is
what told us this was a loader-arch problem and not a runtime crash.

---

ReviANGLE — built and maintained by **Reviusion**.
GitHub: <https://github.com/Reviusion/ReviANGLE>
