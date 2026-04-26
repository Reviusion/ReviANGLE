# ReviANGLE v1.0.1 — Compatibility Hotfix

**Release date:** April 26, 2026

This is a **mod-compatibility hotfix** release. No new performance modules,
no behaviour changes for vanilla GD. The focus is on hardening the OpenGL
proxy so that Geode mods which drive OpenGL aggressively cannot take the
whole game down with them.

If you only run vanilla GD, **v1.0.0 and v1.0.1 are functionally identical**.

---

## Highlights

- **No more crashes when toggling fullscreen with overlay/menu mods active.**
- **No more crashes from mods that write GL uniforms or upload buffers
  before fully setting up GL state.**
- **Mods that ship ImGui-style overlays now compile their shaders correctly**
  under ANGLE D3D11 — the menu actually opens on machines that previously
  hung at start.
- Eight other defensive guards added across the GL proxy.

---

## Global summary of what we fixed

ReviANGLE proxies the desktop GL API on top of Google ANGLE's GLES backend.
ANGLE is **stricter** than desktop GL: a call sequence that desktop drivers
silently no-op (e.g. `glUniform1i` with no program bound) will null-deref deep
inside `libGLESv2.dll` and kill the game. Many Geode mods rely on the
desktop-GL "tolerant" behaviour and crash only when ReviANGLE is active.

v1.0.1 adds a **defensive layer** between mods and ANGLE that:

1. Mirrors desktop-GL "silently no-op on bad state" semantics for the
   commonly-misused calls (uniforms, buffer storage, renderbuffer storage).
2. Translates desktop GLSL idioms (`#version 130/330`,
   `GL_ARB_explicit_attrib_location`, …) into ESSL3 equivalents that
   ANGLE accepts.
3. Tracks GL state across `wglMakeCurrent` (fullscreen toggle) so dedup
   caches don't carry stale bindings into a freshly-created context.
4. Catches the `__fastfail` / `ud2` instruction that ANGLE emits inside
   draw calls when a mod feeds it a vertex format it can't translate, so
   one bad draw doesn't kill the frame.

---

## Detailed changes

### 1. `glUniform*` — null-program guard

**Class of mods affected:** anything that writes uniforms outside of a
strict bind-program → set-uniforms → draw cycle.

**Symptom:** GD crashes the moment the offending mod runs its first
frame. Stack ends in `libGLESv2!GL_Uniform* + 0xb7`, null deref at
offset `+0x1F0`.

**Cause:** ANGLE assumes `GL_CURRENT_PROGRAM` is a valid object inside
every `glUniform*`. Some mods write uniforms before / between program
binds. Desktop GL silently no-ops; ANGLE crashes.

**Fix:** `gd_noProgramBound()` cache-only check at the top of every
`gl_glUniform*` family function. The cache is kept in sync by:
- Querying `GL_CURRENT_PROGRAM` after every `gl_glUseProgram` (catches
  ANGLE rejecting an invalid program ID).
- Clearing the cache from `gl_glDeleteProgram` when the deleted program
  was the bound one.

### 2. `glBufferData` / `glBufferSubData` — null-buffer guard

**Class of mods affected:** anything that uploads vertex/index data
without first binding a buffer to the matching target.

**Symptom:** Intermittent null-deref inside ANGLE's buffer upload path.

**Cause:** ANGLE crashes when `glBufferData` is called with no buffer
bound to the target. Desktop GL silently no-ops.

**Fix:** Per-target binding map (`g_bufferBindings[8]`); both
`gl_glBufferData` and `gl_glBufferSubData` bail out with no-op when the
target slot is `0`/sentinel.

### 3. `glRenderbufferStorage` — null-RBO guard + cache invalidation

**Class of mods affected:** anything that hooks into `CCEGLView::toggleFullScreen`
or otherwise triggers GL context recreation (custom resolution mods,
overlay mods that re-init their FBOs on resize, etc.).

**Symptom:** GD crashes when toggling fullscreen. Stack:
`cocos2d::CCEGLView::toggleFullScreen` → `updateWindow` →
`glRenderbufferStorage` → ANGLE crash at offset `+0xB8`.

**Cause:** Two bugs combined:
1. `CCEGLView::updateWindow` doesn't always re-bind a renderbuffer in
   the freshly-created context before calling `RenderbufferStorage`.
2. Our previous `gl_glBindRenderbuffer` had a thread-local dedup cache
   that **survived context recreation**. After `wglMakeCurrent` to a
   new context, a "rebind to the same ID" got dedup-skipped, but
   ANGLE's new context had nothing bound.

**Fix:**
- Removed dedup from `gl_glBindRenderbuffer` and `gl_glBindFramebuffer`
  (these binds are not on the hot path; correctness > 1 % perf).
- Added `gd_noRBOBound()` defensive guard for `glRenderbufferStorage`
  and `glRenderbufferStorageMultisample`.
- New `gdangle_invalidateAllStateCaches()` is called from
  `wgl_wglMakeCurrent` whenever the EGL context changes — drops every
  thread-local dedup cache (`g_currentProgram`, `g_currentVAO`,
  `g_bufferBindings[8]`, `g_currentRBO`, VAP cache).

### 4. Shader source translator — `#version 130 → 300 es`

**Class of mods affected:** any mod that ships its own shaders or embeds
a UI library (ImGui-based menus, custom particle systems, post-processing
effects).

**Symptom:** Menu / overlay never appears; stderr full of
`'in' : storage qualifier supported in GLSL ES 3.00 and above only` or
`'#version' : directive must occur before any non-preprocessor tokens`.

**Cause:** Most desktop UI libraries generate shaders with `#version 130`
or higher (desktop GL 3.0+); ANGLE only accepts ESSL `100`, `300 es`,
`310 es`, `320 es`. Our previous `gl_glShaderSource` only prepended a
precision qualifier — it didn't translate the version directive at all,
and (worse) it placed the qualifier *before* `#version`, which is itself
a hard error.

**Fix:** Rewrote `gl_glShaderSource` shader translator:
- Detect existing `#version`. Map desktop versions to ES:
  - `110` / `120` (attribute/varying) → `#version 100`
  - `130` / `140` / `150` / `330` / `400+` (in/out) → `#version 300 es`
  - Anything already `… es` → kept verbatim
- Inject `precision mediump float; precision mediump int;` **after** the
  `#version` line and **after** all consecutive `#extension` directives.
  ESSL3 forbids non-preprocessor tokens before `#extension`.
- Strip desktop-only ARB extensions that have no ES counterpart or whose
  feature is built into ES 3.00:
  `GL_ARB_explicit_attrib_location`, `GL_ARB_explicit_uniform_location`,
  `GL_ARB_separate_shader_objects`, `GL_ARB_shading_language_420pack`,
  `GL_ARB_enhanced_layouts`, `GL_ARB_uniform_buffer_object`,
  `GL_ARB_texture_rectangle`, `GL_ARB_sample_shading`,
  `GL_ARB_gpu_shader5`.
  Stripped lines are replaced with `// stripped (desktop-only): …` to
  preserve line numbers in error messages.

### 5. `glDraw{Arrays,Elements,ElementsBaseVertex}` — broken-program & SEH guard

**Class of mods affected:** mods that ship advanced shaders (custom
post-processing, render-passes, GPGPU-style effects), and any mod that
resolves GL functions via direct `GetProcAddress(libGLESv2.dll, …)`
instead of `wglGetProcAddress`.

**Symptom:** Crash inside `glDrawElements` /  `glDrawArrays` with
`EXCEPTION_ILLEGAL_INSTRUCTION` deep in libGLESv2's stream translator.

**Cause:** Two paths feed into the same crash:
1. Mod's shaders use desktop-only syntax that ESSL3 doesn't support
   (e.g. `layout(location=N) uniform`, integer attribute streams,
   `gl_FragColor` writes in a `#version 300 es` shader). The shader
   never compiles, the program never links, but the mod proceeds to
   draw with it. ANGLE responds by hitting an `UNREACHABLE()` deep in
   its D3D11 stream translator.
2. Some mods bypass our proxy entirely for shader / program management
   by resolving function pointers directly from `libGLESv2.dll`. We
   can't see their broken shaders to patch them — we can only
   intercept the eventual draw call.

**Fix (two layers):**
- `gdangle_currentProgramOK()` queries `GL_CURRENT_PROGRAM` from ANGLE
  on each draw and looks up its `GL_LINK_STATUS` (cached per program
  ID, thread-local hot-path cache so it costs ~1 ns when the bound
  program doesn't change). Bails with no-op when the bound program is
  known to have failed linking.
- Wrapped the actual ANGLE call in `__try / __except` SEH that catches
  `EXCEPTION_ILLEGAL_INSTRUCTION` and `EXCEPTION_ACCESS_VIOLATION`,
  logs the first 16 occurrences to `angle_log.txt`, and skips the
  draw. ANGLE's internal C++ state may leak (held mutex, half-mutated
  buffer) but the game survives.

Applied to: `gl_glDrawArrays`, `gl_glDrawElements`,
`gl_glDrawElementsBaseVertex`.

### 6. Other small fixes

- `gl_glBindBuffer` now updates a file-scope `g_bufferBindings[8]` (was
  previously local-static), letting `glBufferData` and the VAP cache
  inspect current bindings.
- VAP cache (`g_vapCache[16]`) and array-buffer cache
  (`g_vapCacheArrayBuffer`) are now cleared by
  `gdangle_invalidateAllStateCaches()` on context switch.
- Five additional GL exports (`glDrawBuffer`, `glBindSampler`,
  `glBlendEquationSeparate`, `glGetVertexAttribiv`,
  `glGetVertexAttribPointerv`) for mods that resolve them via
  `wglGetProcAddress`; without these, those mods bailed with
  `ERROR_PROC_NOT_FOUND` on attach.

---

## What still won't work after v1.0.1

These are inherent ANGLE / GLES 3.0 limitations, not bugs in our proxy:

- **Mods whose shaders use `layout(location=N) uniform`.** This syntax is
  `#version 330+` desktop or `GL_ARB_explicit_uniform_location` only;
  ESSL 3.00 has no equivalent. v1.0.1 prevents the crash and silently
  drops the bad draws — the rest of the mod continues to work, but the
  affected render passes won't display. The mod author has to remove
  the explicit `location` qualifier and switch to `glGetUniformLocation`.
- **Mods that read pixels via persistent / buffer-mapped PBOs**
  (e.g. screen recorders that use `glMapBufferRange` for async
  capture). ANGLE D3D11 doesn't fully implement persistent mapping.
  Recording will produce black or stale frames; the game will not
  crash.
- **Mods that use `glDrawElementsInstancedBaseVertex` with non-zero
  baseVertex.** GLES has no equivalent; we shim it as
  `glDrawElementsInstanced`, which is correct only for `baseVertex == 0`.
- **Anything that requires desktop GL ≥ 3.2 features without an ES path**
  (geometry shaders, tessellation, compute outside ES 3.10+, etc.).

A mod failing in any of these ways is no longer a *crash*; it is at most
a missing visual feature.

---

## Files touched

```
src/gl_proxy.cpp         + SEH wrapper, gdangle_currentProgramOK draw guard
src/gl_proxy_ext.cpp     + null-program / null-buffer / null-RBO guards
                         + shader translator (version + extension + precision)
                         + program link-status tracking
                         + state-cache invalidation hook
src/wgl_proxy.cpp        + wgl_wglMakeCurrent calls invalidateAllStateCaches
                           on context unbind / switch
release/opengl32.dll     rebuilt (~443 KB)
docs/INSTALLATION.md     v1.0.0 → v1.0.1
README.md                v1.0.0 → v1.0.1 in performance table
.github/ISSUE_TEMPLATE/  example version bumped
```

No new public API. No new config flags. Drop-in replacement for v1.0.0's
`opengl32.dll`.

---

## Upgrade

1. Close GD.
2. Replace your existing `C:\Geometry Dash\opengl32.dll` with the one
   from `ReviANGLE-v1.0.1-win64.zip`.
3. Launch.

If anything regresses vs. v1.0.0 on vanilla GD, please open an issue with
your `angle_log.txt`.
