# Installing ReviANGLE

## TL;DR

1. **Backup** your `Geometry Dash` folder (or at minimum the existing `opengl32.dll` if any).
2. Download the latest [Release](https://github.com/Reviusion/ReviANGLE/releases) ZIP.
3. Unzip into your GD install folder (where `GeometryDash.exe` lives).
4. Run `gd-angle-editor.exe` to tune (optional), or just launch GD.

## Step-by-step

### 1. Find your Geometry Dash folder

The path depends on how you installed GD:

| Installation | Default path |
|--------------|--------------|
| Steam | `C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash\` |
| Epic Games | `C:\Program Files\Epic Games\GeometryDash\` |
| Standalone | wherever you put it |

You should see `GeometryDash.exe` in this folder.

### 2. Back up

Either:
- Copy the entire `Geometry Dash` folder to a safe spot, **or**
- At minimum, if there's already an `opengl32.dll` (e.g. from another mod), rename it to `opengl32.dll.backup`

### 3. Download the release

Go to https://github.com/Reviusion/ReviANGLE/releases and download `ReviANGLE-vX.Y.Z-win64.zip`.

### 4. Unzip

The ZIP contains:
```
ReviANGLE-v1.0.0-win64.zip
├── opengl32.dll              ← the proxy mod
├── libEGL.dll                ← ANGLE
├── libGLESv2.dll             ← ANGLE
├── d3dcompiler_47.dll        ← ANGLE
├── angle_config.ini          ← config (editable)
├── gd-angle-editor.exe       ← GUI configurator
├── README.txt
└── LICENSE
```

Extract all of them directly into the GD folder so they end up next to `GeometryDash.exe`. Your folder should look like:

```
Geometry Dash/
├── GeometryDash.exe
├── opengl32.dll              ← from ReviANGLE
├── libEGL.dll                ← from ReviANGLE
├── libGLESv2.dll             ← from ReviANGLE
├── d3dcompiler_47.dll        ← from ReviANGLE
├── angle_config.ini          ← from ReviANGLE
├── gd-angle-editor.exe       ← from ReviANGLE (optional GUI)
├── Resources/                ← original GD
└── ...                       ← other original GD files
```

### 5. (Optional) Configure

Run `gd-angle-editor.exe`. The GUI shows every option with bilingual descriptions, current value, and impact estimates. Save your config when done.

Or edit `angle_config.ini` in any text editor — every option has full bilingual comments.

The shipped default is the **best-feel preset** for the developer's tested hardware (Intel i5-3230M + GT 630M, 90 Hz). If you have different hardware, the most important option to retune is `frame_pacing_target` — see the comment in the file.

### 6. Launch GD

If everything works, you'll see:
- A new file `angle_log.txt` next to `GeometryDash.exe` after first launch (mod's diagnostic log).
- Smoother gameplay, higher FPS, less input lag.

## Verifying it's working

Check `angle_log.txt`. Successful first lines look like:
```
[ReviANGLE] DllMain DLL_PROCESS_ATTACH
ReviANGLE attached \u2014 84 boost modules, backend=d3d11  (by Reviusion)
gpu_forcer: NvOptimusEnablement export installed
nvapi: using app profile for GeometryDash.exe
nvapi: PREFERRED_PSTATE=PreferMax = 0x00000000 applied
nvapi: DRS settings saved (5/5 applied)
workingset_lock: HARD floor=384 MB ceiling=1536 MB
frame_pacing: target dt = 8.333 ms (= 120 FPS), high-res waitable timer ENABLED (no CPU spin)
low_latency: MaxFrameLatency=1 (was 3)
gpu_thread_prio: GPU thread priority = +7 (max)
```

If you see `ANGLE init failed` or similar, see [Troubleshooting](#troubleshooting).

## Uninstalling

Just delete the files you added:
- `opengl32.dll`
- `libEGL.dll`
- `libGLESv2.dll`
- `d3dcompiler_47.dll`
- `angle_config.ini`
- `gd-angle-editor.exe`
- `angle_log.txt` (generated)
- `shader_cache/` folder (if it exists, generated)
- `plist_cache/` folder (if it exists, generated)

If you renamed an original `opengl32.dll.backup`, rename it back.

## Troubleshooting

### GD won't start at all

**Most common cause**: missing ANGLE DLL. Verify all 3 of these are next to `GeometryDash.exe`:
- `libEGL.dll`
- `libGLESv2.dll`
- `d3dcompiler_47.dll`

If `angle_log.txt` doesn't appear, the mod's `DllMain` never ran — usually means the DLL is corrupted or for the wrong architecture. Re-download.

### GD starts but black screen / no rendering

Open `angle_config.ini` and try:
```ini
backend=d3d11    # Should be d3d11 by default. If it isn't, fix it.
debug=true       # Enables verbose logging — very useful for diagnosing
```

Then check `angle_log.txt` for backend selection and any `glViewport` / shader errors.

If still black, try `backend=d3d9` (works on weaker / older drivers).

### Lower FPS than vanilla GD

This usually means `frame_pacing_target` is too high for your GPU.

- If your **peak uncapped FPS** during effects is, say, 110 → set `frame_pacing_target=90` (with headroom).
- If you don't know your peak FPS: set `frame_pacing=false` once, run a hard level, watch FPS counter, then re-enable pacing with target ~ 80 % of your worst-case FPS.

See the long bilingual comment around `frame_pacing_target` in `angle_config.ini` — it has a full explanation with budget math.

### Crashes on launch with NVAPI errors in log

Set `nvapi_profile=false` in `angle_config.ini`. NVAPI tries to write to your Nvidia profile DB; some setups (e.g. corporate-managed PCs) deny this.

### Crashes on launch with "DLL search path" errors

Some antivirus / Windows Defender heuristics flag unsigned DLL injection. Add the GD folder to AV exceptions.

### "Failed to write to memory" crash inside `d3d11.dll`

This was a known old bug from `low_latency` calling the wrong vtable slot. Make sure you're running the latest release (check `angle_log.txt` first line).

### Online features broken

The default config sets `online_block_gameplay=true` (blocks network calls during levels for stability). To restore online features, set it to `false`.

### Crashes only on AMD / Intel GPUs

Disable the Nvidia-specific tweaks:
```ini
nvapi_profile=false
gpu_forcer=false
```

### Other mods stop working

ReviANGLE replaces `opengl32.dll`, which conflicts with any other mod that also replaces it. Known coexisting mods: **Eclipse Menu**. Known untested: **Geode**, **MegaHack v7**, **Mega Hack Pro**.

## Getting help

If none of the above fixes your issue:
1. Set `debug=true` in `angle_config.ini`.
2. Reproduce the issue.
3. Open a [GitHub Issue](https://github.com/Reviusion/ReviANGLE/issues/new/choose), attach `angle_log.txt`, and describe what happened.
