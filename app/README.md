# Visual — current two-screen alpha

This folder contains the integrated Visual application runtime.

## Current purpose

The current alpha is a two-screen low-vision magnifier and experimental platform:

- primary/source display remains the work/context surface;
- another physical display becomes the magnified Detail surface;
- Detail can follow deliberate pointer movement, text caret or keyboard focus;
- viewport motion uses comfort margins rather than constant recentering;
- the selected target is highlighted on Detail;
- Detail does not steal keyboard focus from the source application.

This is not yet the four-screen Doxa product and is not intended to become a broad ZoomText/SuperNova replacement.

## Build

From PowerShell:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\app\build.ps1
```

Release executables:

- `app\build\Release\visual_app.exe`
- `app\build\Release\visual_diagnostics.exe`

The build version can be overridden, for example:

```powershell
.\app\build.ps1 -Configuration Release -Version 0.1.0-alpha.1
```

The canonical default semantic version/package identity is in `app\version.cmake`.

Current Release build runs the production CTest suite, including diagnostics JSON validation.

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

WGC frame arrival is deliberately **not** the clock for pointer/caret interaction anymore. Cursor capture is disabled, so the app independently samples POI evidence and renders the latest cached source texture on a steady cadence.

This change was made after human use exposed visible lag during fast pointer movement over otherwise static source content.

## Default display behavior

With two or more displays:

- source defaults to the Windows primary monitor;
- destination defaults to the first different active monitor;
- Detail uses the destination monitor full-screen.

Optional command-line arguments:

- `--source N`;
- `--dest N`;
- `--zoom 1|2|4`;
- `--log PATH`;
- `--single-monitor` for development only.

Monitor index is currently an engineering selection mechanism, not the future Doxa identity/role model.

## Global alpha hotkeys

These work while another application retains focus:

- `Ctrl+Alt+1` — 1x;
- `Ctrl+Alt+2` — 2x;
- `Ctrl+Alt+4` — 4x;
- `Ctrl+Alt+0` — temporary normal view / exact return to previous magnified viewport;
- `Ctrl+Alt+T` — tracking on/off;
- `Ctrl+Alt+Q` — exit Visual.

## Tracking evidence

Current candidate sources:

1. explicit user target architecture slot;
2. recently moving pointer;
3. UIA TextPattern2 caret;
4. UIA TextPattern insertion range;
5. Win32 caret;
6. UIA keyboard focus;
7. Win32 keyboard focus.

A moving pointer is treated as short-lived direct user intent. When movement stops, caret/focus evidence resumes automatically.

A transient UIA semantic-caret miss may retain the last real caret with its **original timestamp** only; normal freshness rules still expire it. Cached semantic evidence is never retimestamped to pretend that it is new.

## Interaction-cadence evidence

After the 2026-09-23 cadence refactor:

- fast-pointer stress: pointer position error p95 0 px;
- pointer evidence age p95 ~16.6 ms;
- pointer -> semantic-caret handoff ~88.7 ms;
- 60-second soak: ~59.94 fps;
- soak frame-gap p95 ~16.8 ms, max ~24.8 ms;
- zero Present failures in those runs.

Human regression is still required to confirm that the previously perceived fast-pointer lag is gone perceptually.

## Viewport policy

Current experimental defaults:

- 20% horizontal comfort margin;
- 20% vertical comfort margin;
- hold while the target remains inside the comfort area;
- minimum required pan when it leaves;
- jump classification for large moves;
- source-bound clamping.

These values are not final product defaults and need low-vision task-use tuning.

## Locator

The selected POI is rendered with a provider-neutral two-tone high-contrast locator:

- pointer — crosshair-style mark;
- caret/focus — outlined target region.

Current black/white styling is an alpha default, not a final personalization model.

## Telemetry

The CSV preserves the original integrated fields and now also records:

- pointer movement age;
- UIA snapshot age;
- current UIA caret/focus availability;
- selected POI age;
- selected POI screen rectangle.

These fields are intended to make pointer/caret handoff failures diagnosable rather than inferred from visible behavior alone.

## Structured diagnostics

`visual_diagnostics.exe` outputs a small JSON support report containing Visual version, Windows build, active monitor geometry/mode/DPI, GPU adapter names and adjacent Visual-binary presence/size.

It intentionally does not collect screenshots, document/window/browser contents, usernames, arbitrary file listings, environment-variable dumps, passwords or tokens.

See `app\packaging\README.md` for support-bundle use.

## Alpha packaging and releases

Velopack is pinned through the repository-local .NET tool manifest and is used for the `Standivarius.Visual` `alpha` channel.

Packaging/release infrastructure lives under:

- `app\packaging\`
- `.config\dotnet-tools.json`
- `.github\workflows\release-alpha.yml`

The normal native build does not require .NET/Velopack. GitHub Actions installs the SDK, restores the pinned `vpk` CLI, builds/tests Visual and publishes alpha installer/update assets to GitHub Releases.

Early infrastructure releases are intentionally unsigned; Windows may show unknown-publisher/SmartScreen warnings until trusted code signing is added.

The release pipeline is separate from in-app automatic update integration. Do not claim automatic updating is proven until an installed alpha successfully updates to a later alpha using the Velopack C/C++ update client.

## Browser status

Browser editing-caret support is **not yet validated**.

A deterministic Microsoft Edge fixture renders correctly and Visual has observed valid Edge UIA TextPattern caret evidence transiently. During the measured automated editing phases, however, caret evidence has not remained reliably available and Visual falls back to browser/window focus.

Treat this as an explicit compatibility investigation. Do not claim reliable Chromium caret tracking yet.

## Recovery policy

- WGC content-size changes recreate the frame pool;
- capture-item closure is detected;
- display topology changes are detected;
- topology invalidation causes a clean shutdown with a distinct telemetry/exit reason;
- after the desired two-display topology returns, relaunch Visual and it rebuilds current capture/render state.

This policy has been physically validated with a real HDMI disconnect/reconnect cycle.

## Context / productivity experiment

The first Context+Detail experiment protocol is prepared at:

`lab\experiments\context_persistence\PROTOCOL.md`

The first comparison uses the current application before adding more features:

1. conventional one-screen magnification;
2. Visual two-screen dynamic Detail + source/context;
3. ordinary two-screen extended desktop.

A dedicated Freeze/Reference feature should only move forward after this comparison shows what additional navigation cost actually remains.

## Known alpha limitations

- human regression after the fast-pointer cadence fix is pending;
- reliable Chromium/Edge editing-caret tracking is pending;
- display sleep/wake has not yet been exercised separately from HDMI unplug/reconnect;
- viewport margins and locator appearance have not yet had a low-vision usability-tuning pass;
- installer/update infrastructure is initial and unsigned; production signing and in-app update validation remain;
- no polished launcher/settings/onboarding UI;
- no persistent Doxa monitor roles/workspace recall yet;
- no four-screen Doxa validation yet.
