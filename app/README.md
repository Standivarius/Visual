# Visual - low-vision workspace alpha

This folder contains the integrated Visual Windows application runtime used for the Doxa low-vision workstation project.

## Current purpose

Visual is currently a functional low-vision workspace alpha:

- a Context display remains the source/work surface and shows the current Detail View region;
- a Detail display shows the magnified working view;
- Detail follows deliberate pointer movement, text caret or keyboard focus;
- the viewport uses comfort margins rather than constant recentering;
- pointer, caret and focus tracking/markers can be controlled independently;
- Detail is non-activating so the source application keeps keyboard focus; settings and display roles persist across launches.

It is an experimental magnification/productivity platform, not a finished general-purpose magnifier replacement.

## Build

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\app\build.ps1 -Configuration Release
```

Release outputs:

- `app\build\Release\visual_app.exe`
- `app\build\Release\visual_diagnostics.exe`
- `app\build\Release\velopack_libc.dll`

The canonical default version is `0.1.0-alpha.6` in `app\version.cmake`. A build can override it with `-Version <semver>`.

The native build acquires the pinned Velopack 1.2.0 C/C++ SDK into the ignored `app\third_party\velopack\` cache, builds Visual, and runs seven CTest tests. Ordinary native builds do not require a .NET SDK.

## Runtime architecture

```text
Windows Graphics Capture
  -> latest source texture cache

steady interaction/render loop (~60 Hz)
  -> Win32 pointer/caret/focus evidence
  -> UIA caret/focus evidence
  -> freshness + intent arbitration
  -> viewport controller
  -> locator model
  -> D3D11 crop / scale / locator
  -> second-screen Detail
```

WGC frame arrival is deliberately not the interaction clock. Cursor capture is disabled; Visual samples POI evidence independently and renders the latest cached source texture on a steady cadence.

## Display behavior

With two or more displays:

- Context defaults to the Windows primary monitor;
- Detail defaults to the first different active monitor;
- Context and Detail roles persist in `%LOCALAPPDATA%\Standivarius\Visual\settings.ini`;
- an optional Reference role can be assigned to another display and remains available for normal Windows content;
- screen-role changes are saved and take effect on the next Visual start;
- Detail uses the assigned destination monitor full-screen.

Engineering command-line options include:

- `--source N`
- `--dest N`
- `--zoom 1|1.5|2|3|4`
- `--log PATH`
- `--single-monitor`

Command-line monitor indices remain engineering overrides. Normal runs use the persisted screen roles.

## Global alpha hotkeys

- `Ctrl+Alt+1` - 1x
- `Ctrl+Alt+2` - 2x
- `Ctrl+Alt+3` - 3x
- `Ctrl+Alt+4` - 4x
- `Ctrl+Alt+0` - temporary normal view / exact return
- `Ctrl+Alt+T` - follow activity on/off
- `Ctrl+Alt+S` - open Visual Settings
- `Ctrl+Alt+Q` - exit Visual

The settings UI also provides 1.5x magnification. Right-click the Detail window for the compact Visual menu.

## Visual Settings and Context view

`Visual Settings` is a native keyboard-accessible Win32 settings window. It persists under `%LOCALAPPDATA%\Standivarius\Visual\settings.ini` and exposes a deliberately small user-facing set rather than engineering timing parameters:

- magnification: 1x, 1.5x, 2x, 3x or 4x;
- master follow-activity plus independent pointer, text-caret and keyboard-focus following;
- independent high-visibility pointer, caret and focus markers;
- a Context-screen **Detail View** rectangle showing the exact source region currently enlarged on Detail, with optional translucent shading;
- Normal, High contrast, Inverted colours and Grayscale appearance modes on Detail;
- persisted Context, Detail and optional Reference screen roles.

The Detail View indicator is a separate layered window marked `WDA_EXCLUDEFROMCAPTURE`, so it stays visible on Context without being recursively captured into Detail. If Windows cannot apply capture exclusion, Visual does not show the indicator.

Tracking, marker, magnification, appearance and Detail View settings apply live. Physical Context/Detail/Reference role changes are restart-bound by design so the active Windows Graphics Capture session is not rebuilt from the settings dialog.

## Tracking and viewport evidence

Candidate POI sources include recent pointer movement, UIA TextPattern caret, Win32 caret, UIA keyboard focus and Win32 keyboard focus. Recent pointer movement is treated as short-lived direct intent; when it stops, semantic evidence can resume. Cached semantic evidence keeps its original timestamp.

Current viewport defaults use 20% horizontal and vertical comfort margins, minimum required panning when a target leaves that region, jump classification for large moves and source-bound clamping. These are experimental defaults, not final low-vision product settings.

## Pointer regression status

The PMv2-correct physical regressions on 2026-09-24 closed the prior generic pointer-loss investigation for the current build:

- two real 1920x1080 displays;
- independent `GetCursorPos` ground truth;
- Visual telemetry with flushed lifecycle markers;
- full-desktop video/contact-sheet review;
- zero Present failures;
- zero locator-missing frames during slow horizontal, fast horizontal, fast zig-zag and edge-teleport phases;
- no genuine rendered-locator disappearance reproduced.

The source-to-Detail monitor boundary is a separate, understood policy issue. When the native cursor leaves the source monitor, `Win32EvidenceProvider` stops emitting it as a source POI, arbitration falls back to semantic focus, and Detail can jump. Pointer POI resumes immediately when the cursor returns to the source. This is not a capture/render failure; it needs a deliberate UX policy decision.

## Telemetry

Runtime CSV telemetry records render state, POI source, locator visibility, viewport geometry and freshness information. Startup/capture markers are flushed immediately:

- `telemetry_started`
- `capture_started`
- `first_source_frame`
- `first_render_frame`
- `capture_failure`

The first numeric telemetry frame is also flushed immediately, preventing force-terminated regression runs from producing misleading zero-byte evidence.

## Velopack lifecycle

Visual pins Velopack 1.2.0. `app\src\velopack_entry.cpp` owns the real `wWinMain` and executes:

```cpp
Velopack::VelopackApp::Build().Run();
```

before normal Visual DPI/WinRT/display/capture initialization. The existing implementation in `main.cpp` is compiled with its `wWinMain` symbol renamed to `VisualProductMain` and is called only after Velopack lifecycle handling returns.

The SDK archive contains `velopack_libc_win_x64_msvc.dll`, but its import library encodes the runtime dependency name `velopack_libc.dll`. The build therefore copies/renames the SDK DLL to `velopack_libc.dll` beside `visual_app.exe`, and packaging ships that imported name.

Local `0.1.0-alpha.2` lifecycle verification is complete:

- Release build linked successfully;
- CTest 5/5 passed;
- normal `vpk 1.2.0` packaging succeeded with no `--skipVeloAppCheck`;
- the full package contains `lib/app/velopack_libc.dll`;
- clean Setup exited `0`;
- the Velopack install hook reported `Hook executed successfully`;
- installed ProductVersion is `0.1.0-alpha.2`;
- installed `Update.exe` and `velopack_libc.dll` are present;
- installed `visual_app.exe --update-check` exited `0` and reported `result=no_update` against the current public alpha feed;
- the hardened consolidated verifier reports `LIFECYCLE_RESULT=PASS`.

## Update maintenance

The installed executable exposes engineering-only maintenance commands that do not enter Visual's normal capture/UI path:

```powershell
visual_app.exe --update-check
visual_app.exe --update-now
```

`--update-check` uses Velopack's native GitHub update source for `Standivarius/Visual` with prereleases enabled. Exit `0` means no newer release; exit `10` means an update is available.

`--update-now` checks, downloads and schedules an available update for application after the process exits, without an automatic restart. This path is now proven end to end: installed public alpha.2 detected public alpha.3, downloaded it, scheduled apply, and the installed ProductVersion changed to `0.1.0-alpha.3`. The updated alpha.3 then reported `result=no_update` and launched/exited normally through `Ctrl+Alt+Q`.

Maintenance diagnostics are appended to `%LOCALAPPDATA%\Standivarius\Visual\logs\visual_update.log`.

## Packaging and releases

See `app\packaging\README.md` for the pinned packaging toolchain and release procedure. The package script uses normal Velopack application validation and does not bypass the lifecycle check.

Early alpha packages are unsigned, so Windows may show unknown-publisher/SmartScreen warnings until trusted code signing is added.

## Structured diagnostics

`visual_diagnostics.exe` reports Visual version/package/channel, Windows build, process architecture, monitor geometry/mode/DPI, GPU adapter names and adjacent Visual binary presence/size. It intentionally does not collect screenshots, application contents, usernames, environment-variable dumps, passwords or tokens.

## Browser status

Reliable Chromium/Edge editing-caret tracking is not yet validated. A deterministic Edge fixture has exposed valid UIA TextPattern caret evidence transiently, but measured editing phases can still fall back to browser/window focus.

## Recovery policy

WGC size changes recreate the frame pool; capture-item closure and display-topology invalidation are detected; topology invalidation causes a clean shutdown; relaunch after topology restoration rebuilds capture/render state. HDMI disconnect/reconnect recovery has been physically validated.

## Known alpha limitations

- reliable Chromium/Edge editing-caret tracking remains open;
- display sleep/wake is not separately validated from HDMI disconnect/reconnect;
- viewport margins and locator appearance need low-vision usability tuning;
- source-to-Detail pointer-boundary UX policy needs a product decision;
- trusted code signing is not configured;
- a basic accessible Visual Settings UI now exists; installer/onboarding/update-prompt UI remains intentionally separate and minimal;
- user-facing update prompts/background update policy are still not implemented;
- Context, Detail and optional Reference roles persist by Windows display device name; Doxa-specific hardware identity and validated user presets still require the real Doxa and user research;
- broader Doxa multi-display validation remains future work.
