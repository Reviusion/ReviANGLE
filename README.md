<div align="center">

# ReviANGLE

**A drop-in `opengl32.dll` proxy that routes Geometry Dash's OpenGL through Google ANGLE → DirectX 11.**

*Unlock FPS, reduce input lag, eliminate microstutters — all on hardware Geometry Dash never officially targeted.*

[![Build Windows](https://github.com/Reviusion/ReviANGLE/actions/workflows/build.yml/badge.svg)](https://github.com/Reviusion/ReviANGLE/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Release](https://img.shields.io/github/v/release/Reviusion/ReviANGLE)](https://github.com/Reviusion/ReviANGLE/releases)

[English](#english) · [Русский](#russian)

</div>

---

## English

### What is it?

ReviANGLE is a performance mod for **Geometry Dash 2.2** that replaces the game's `opengl32.dll` with a custom proxy. The proxy:

1. **Translates OpenGL → DirectX 11** via [Google ANGLE](https://chromium.googlesource.com/angle/angle).
2. Adds **84 low-level performance modules** that hook into ANGLE's hot path:
   - GL state deduplication (skip 30-50 % of redundant cocos2d-x calls)
   - High-resolution frame pacing (no CPU spin)
   - DXGI low-latency present (1-frame queue)
   - Optional half-resolution rendering with linear upscale (~30-50 % GPU win on weak GPUs)
   - Optional idle-frame Present elision (saves GPU power on menus)
   - NVAPI driver profile (PSTATE=P0, max-perf, no driver vsync)
   - Working-set lock so Windows doesn't page our hot data out
   - GPU thread priority bump
   - 40+ other tweaks — see [`docs/CONFIG_REFERENCE.md`](docs/CONFIG_REFERENCE.md)
3. Ships with **ReviANGLE Studio** — a standalone GUI configurator with bilingual (EN/RU) descriptions for every option.

### Why?

Vanilla GD was written for OpenGL on hardware that's now **15+ years old**. On weak laptops (Intel HD / GT 630M / etc.) it stutters, caps at 60 FPS, and wastes huge amounts of CPU on redundant driver calls. ReviANGLE rewrites that pipeline.


### Quick install

1. Go to [**Releases**](https://github.com/Reviusion/ReviANGLE/releases) and download `ReviANGLE-vX.Y.Z-win64.zip`.
2. **Backup** your `Geometry Dash` folder (or at least the original `opengl32.dll` if it exists).
3. Extract the ZIP into your Geometry Dash install directory (where `GeometryDash.exe` lives).
   You should now have:
   ```
   Geometry Dash/
   ├── GeometryDash.exe
   ├── opengl32.dll              ← from ReviANGLE
   ├── libEGL.dll                ← from ReviANGLE (ANGLE)
   ├── libGLESv2.dll             ← from ReviANGLE (ANGLE)
   ├── d3dcompiler_47.dll        ← from ReviANGLE (ANGLE)
   ├── angle_config.ini          ← config (editable)
   └── gd-angle-editor.exe       ← optional GUI configurator
   ```
4. Launch GD. If everything works, `angle_log.txt` will appear next to the `.exe`.

### Configure

Run **`gd-angle-editor.exe`** for a GUI:

- Bilingual descriptions (English + Russian) for every option
- Enum dropdowns where applicable (e.g. `backend = d3d11 / d3d9 / vulkan`)
- Comments and section structure preserved on save (round-trip safe)
- One-click "Reset to defaults" applies the **best-feel preset** for the tested hardware

Or edit `angle_config.ini` directly — it's plain text with full bilingual comments.

### Performance numbers

Measured on the tested hardware, average over 10 runs of the same hard demon (Acu):

| Build | Avg FPS | 1 % Low | Frame-time variance |
|-------|---------|---------|---------------------|
| Vanilla GD 2.2 | 88 | 41 | high (visible micro-stutter) |
| **ReviANGLE 1.0.0** | **156 (+77 %)** | **121 (+195 %)** | **very low** |

*FPS uncapped (`unlock_fps_cap=true`), pacer at `frame_pacing_target=120`. Your numbers will differ — these are reference figures from the developer's machine.*

### Building from source

See [`docs/BUILDING.md`](docs/BUILDING.md). TL;DR:

```powershell
# Prerequisites: Visual Studio 2022 (C++ workload), CMake 3.20+
git clone https://github.com/Reviusion/ReviANGLE.git
cd ReviANGLE
cmake -B build -A x64         # GD 2.2+ is 64-bit
cmake --build build --config Release
```

Output goes to `build\Release\` — `opengl32.dll` and `gd-angle-editor.exe`.

You'll also need the ANGLE prebuilt binaries (`libEGL.dll`, `libGLESv2.dll`, `d3dcompiler_47.dll`) — see [`docs/BUILDING.md`](docs/BUILDING.md#angle-prebuilts).

### Project layout

```
ReviANGLE/
├── src/                          ← Mod source (proxy DLL)
│   ├── dllmain.cpp               ← DLL entry point + module wiring
│   ├── gl_proxy.cpp / .hpp       ← OpenGL function exports + state dedup
│   ├── gl_proxy_ext.cpp          ← GLES 2/3 functions (uniforms, buffers, FBO)
│   ├── wgl_proxy.cpp             ← WGL → EGL bridge (context creation)
│   ├── angle_loader.cpp / .hpp   ← Loads libEGL.dll / libGLESv2.dll
│   ├── config.cpp / .hpp         ← INI loader for angle_config.ini
│   └── boost_*.cpp               ← 84 individual perf modules (see docs)
├── config_editor/                ← ReviANGLE Studio GUI (Dear ImGui)
│   ├── main.cpp                  ← WinAPI window + DX11 setup
│   ├── editor_app.cpp / .hpp     ← UI logic
│   ├── schema.cpp / .hpp         ← Bilingual option metadata
│   ├── ini_parser.cpp / .hpp     ← Round-trip-safe INI parser
│   └── round_trip_test.cpp       ← CLI test
├── docs/
│   ├── ARCHITECTURE.md           ← How it all hangs together
│   ├── BUILDING.md               ← Build instructions, ANGLE setup
│   ├── INSTALLATION.md           ← End-user install guide
│   └── CONFIG_REFERENCE.md       ← Full reference of every config option
├── examples/                     ← Pre-tuned configs
├── release/                      ← Pre-built binaries (drop-in bundle)
├── .github/                      ← CI + issue templates
├── CMakeLists.txt
├── CONTRIBUTING.md
├── LICENSE                       ← MIT
└── README.md                     ← You are here
```

### Troubleshooting

| Symptom | Likely cause / fix |
|---------|--------------------|
| GD won't start, no `angle_log.txt` | Missing ANGLE DLLs — check `libEGL.dll`, `libGLESv2.dll`, `d3dcompiler_47.dll` are next to `GeometryDash.exe` |
| GD starts but black screen | Backend mismatch — open `angle_config.ini`, set `backend=d3d11` |
| FPS lower with mod than without | Set `frame_pacing_target` ≤ your GPU's worst-case FPS during effects (see config comment) |
| First-level stutter | Enable `shader_warmup=true` |
| Online features broken | Disable `online_block_gameplay` (set to `false`) |
| AMD/Intel GPU crashes | Disable `nvapi_profile` and `gpu_forcer` |

For more, see [`docs/INSTALLATION.md`](docs/INSTALLATION.md).

### Compatibility

| Compatible | Status |
|------------|--------|
| Geometry Dash 2.2 (Steam, standalone) | ✅ tested |
| Eclipse Menu | ✅ tested (compatible) |
| Mac / Linux | ❌ Windows-only |

### Credits & acknowledgements

- **ANGLE** team at Google for the GLES → D3D translation library.
- **cocos2d-x** authors — GD's underlying engine.
- **RobTop Games** — Geometry Dash itself (this mod is unaffiliated).
- **Dear ImGui** — used for the configurator GUI.
- Author: **Reviusion** ([@Reviusion](https://github.com/Reviusion)).

### License

MIT — see [`LICENSE`](LICENSE). You may use, modify, redistribute, and even sell this code, as long as the copyright notice is preserved.

ANGLE binaries are licensed under the [BSD 3-Clause license](https://chromium.googlesource.com/angle/angle/+/refs/heads/main/LICENSE) and are not part of this repository's source — they're bundled in releases for convenience only.

### Disclaimer

This is a **third-party** modification. Use at your own risk. **Always back up your `Geometry Dash` folder** before installing. The author is not affiliated with RobTop Games and is not responsible for save corruption, account bans (none observed in testing, but theoretically possible), or any other adverse effects.

---

## Russian

### Что это?

ReviANGLE — это мод производительности для **Geometry Dash 2.2**, который подменяет `opengl32.dll` игры на свой proxy. Proxy:

1. **Переводит OpenGL → DirectX 11** через [Google ANGLE](https://chromium.googlesource.com/angle/angle).
2. Добавляет **84 низкоуровневых модуля оптимизации**:
   - Дедупликация GL state (пропуск 30-50 % redundant вызовов cocos2d-x)
   - Frame pacing на high-res waitable timer (без CPU spin)
   - DXGI low-latency present (очередь = 1 кадр)
   - Опционально: half-res рендер с линейным апскейлом (~30-50 % GPU выигрыш на слабых GPU)
   - Опционально: пропуск Present на idle-кадрах (экономия мощности GPU на меню)
   - NVAPI driver profile (PSTATE=P0, max-perf, выключение vsync на драйверном уровне)
   - Working-set lock — Windows не выгружает наши горячие страницы под памяти-pressure
   - Boost приоритета GPU thread
   - И ещё 40+ tweaks — [`docs/CONFIG_REFERENCE.md`](docs/CONFIG_REFERENCE.md)
3. Идёт с **ReviANGLE Studio** — отдельным GUI-конфигуратором с двуязычными (EN/RU) описаниями каждой опции.

### Зачем?

Ванильный GD написан под OpenGL на железо **15+ летней давности**. На слабых лаптопах (Intel HD / GT 630M / etc.) он лагает, упирается в 60 FPS и тратит огромное количество CPU на redundant driver calls. ReviANGLE переписывает этот pipeline.


### Быстрая установка

1. Перейди на [**Releases**](https://github.com/Reviusion/ReviANGLE/releases) и скачай `ReviANGLE-vX.Y.Z-win64.zip`.
2. **Сделай бэкап** папки Geometry Dash (или хотя бы оригинального `opengl32.dll`, если он есть).
3. Распакуй ZIP в папку с установленной GD (где лежит `GeometryDash.exe`). Должно получиться:
   ```
   Geometry Dash/
   ├── GeometryDash.exe
   ├── opengl32.dll
   ├── libEGL.dll
   ├── libGLESv2.dll
   ├── d3dcompiler_47.dll
   ├── angle_config.ini
   └── gd-angle-editor.exe       ← опциональный GUI
   ```
4. Запусти GD. Если всё ок — рядом с `.exe` появится `angle_log.txt`.

### Настройка

Запусти **`gd-angle-editor.exe`** для GUI:

- Двуязычные описания (EN+RU) к каждой опции
- Enum-дропдауны там где это уместно (`backend = d3d11 / d3d9 / vulkan`)
- Комментарии и структура секций сохраняются при save (round-trip safe)
- Кнопка "Reset to defaults" возвращает **best-feel preset** для тестового железа

Или редактируй `angle_config.ini` напрямую — это plain text с полными двуязычными комментариями.

### Сборка из исходников

См. [`docs/BUILDING.md`](docs/BUILDING.md). TL;DR:

```powershell
# Требования: Visual Studio 2022 (C++ workload), CMake 3.20+
git clone https://github.com/Reviusion/ReviANGLE.git
cd ReviANGLE
cmake -B build -A x64         # GD 2.2+ это 64-битный процесс
cmake --build build --config Release
```

Сборка попадёт в `build\Release\` — `opengl32.dll` и `gd-angle-editor.exe`.

Также понадобятся ANGLE prebuilt бинарники (`libEGL.dll`, `libGLESv2.dll`, `d3dcompiler_47.dll`) — см. [`docs/BUILDING.md`](docs/BUILDING.md#angle-prebuilts).

### Лицензия

MIT — см. [`LICENSE`](LICENSE). Можно использовать, модифицировать, перепродавать, при сохранении copyright notice.

ANGLE-бинарники лицензированы под [BSD 3-Clause](https://chromium.googlesource.com/angle/angle/+/refs/heads/main/LICENSE) и НЕ являются частью исходников этого репо — они кладутся в релизы только для удобства.

### Дисклеймер

Это **сторонняя** модификация. Используй на свой страх и риск. **Всегда делай бэкап папки `Geometry Dash`** перед установкой. Автор не аффилирован с RobTop Games и не несёт ответственности за повреждения сейвов, баны аккаунта (не наблюдалось при тестировании, но теоретически возможны), или любые другие адверс-эффекты.

---

<div align="center">

Made by **Reviusion** with love and a 13-year-old laptop ❤️

</div>
