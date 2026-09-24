# Visual - current two-screen alpha

This folder contains the integrated Visual Windows application runtime used for the Doxa low-vision workstation project.

## Current purpose

Visual is currently a functional two-screen engineering alpha:

- the Windows primary display is the source/work surface;
- a second physical display becomes the magnified Detail surface;
- Detail follows deliberate pointer movement, text caret or keyboard focus;
- the viewport uses comfort margins rather than constant recentering;
- the selected target is shown with a high-contrast locator;
- Detail is non-activating so the source application keeps keyboard focus.

It is an experimental magnification/productivity platform, not a finished general-purpose magnifier replacement.

## Build

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\app\build.ps1 -Configuration Release
```

Release outputs:

- `app\build\Release\visual_app.exe`
- `app\build\Release\visual_diagnostics.exe`
- `app\build\Release\velopack_libc.dll`

The canonical default version is `0.1.0-alpha.3` in `app\version.cmake`. A build can override it with `-Version <semver>`.

The native build acquires the pinned Velopack 1.2.0 C/C++ SDK into the ignored `app\third_party\velopack\` cache, builds Visual, and runs the five CTest tests. Ordinary native builds do not require a .NET SDK.

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

- source defaults to the Windows primary monitor;
- destination defaults to the first different active monitor;
- Detail uses the destination monitor full-screen.

Engineering command-line options include:

- `--source N`
- `--dest N`
- `--zoom 1|2|4`
- `--log PATH`
- `--single-monitor`

Monitor index is an engineering selection mechanism, not the final Doxa display-role model.

## Global alpha hotkeys

- `Ctrl+Alt+1` - 1x
- `Ctrl+Alt+2` - 2x
- `Ctrl+Alt+4` - 4x
- `Ctrl+Alt+0` - temporary normal view / exact return
- `Ctrl+Alt+T` - tracking on/off
- `Ctrl+Alt+Q` - exit Visual

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

`--update-now` checks, downloads and schedules an available update for application after the process exits, without an automatic restart. That download/apply path is implemented but cannot be called end-to-end proven until a later public alpha exists for an installed alpha.2 to consume.

Maintenance diagnostics are appended to `%LOCALAPPDATA%\Standivarius.Visual\visual_update.log`.

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
- no polished launcher/settings/onboarding/update UI exists;
- update download/apply needs a later published alpha for end-to-end proof;
- persistent Doxa monitor roles/workspace recall are not implemented;
- broader Doxa multi-display validation remains future work.
