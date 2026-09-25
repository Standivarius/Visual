# Visual Support Knowledge

Generated from reviewed source documents under `support/knowledge/visual/`. Each section preserves its source filename and content.

---

## Source: diagnostics.md

# Visual structured diagnostics

## Purpose

`visual_diagnostics.exe` produces a small machine-readable JSON report for support. It is intended to provide enough technical context to distinguish Visual, Windows-display and hardware/environment problems without collecting user content.

## Current schema

The initial report includes:

- `schema_version`;
- `app_id`;
- `visual_version`;
- `release_channel`;
- generation timestamp in UTC;
- process architecture;
- Windows major/minor/build;
- whether an adjacent `visual_app.exe` exists and its file size;
- active monitor count;
- per-monitor device name, bounds, display mode, refresh rate and effective DPI/scaling;
- GPU adapter names.

The installed filesystem path is intentionally not reported because a per-user path can expose the Windows account name.

## Privacy boundary

The diagnostic executable intentionally does not collect:

- screenshots;
- window/document/browser contents;
- usernames;
- arbitrary file listings;
- environment-variable dumps;
- passwords, tokens or API keys.

A support bundle contains `diagnostics.json` plus a privacy note. Runtime CSV telemetry is included only if an explicit telemetry path is supplied by the user/support workflow.

## Interpretation principles

- `active_monitor_count < 2` means the normal two-screen Visual scenario is not currently available, but does not by itself identify the cause.
- Monitor geometry/DPI should be interpreted as Windows currently reports it; do not infer physical placement beyond reported bounds.
- GPU adapter presence does not prove WGC or D3D rendering health.
- The initial diagnostics schema is primarily static/environmental. Runtime health/error codes should be added as the field-support requirements become clearer.

## Support bundle command from a development checkout

```powershell
.\app\packaging\support-bundle.ps1
```

Optional explicit telemetry inclusion:

```powershell
.\app\packaging\support-bundle.ps1 -TelemetryPath C:\path\to\visual_app_telemetry.csv
```

When Visual becomes a polished installed product, expose this capability through a user-facing support action rather than requiring a PowerShell command.

---

## Source: fast-pointer.md

# Fast pointer movement and pointer-to-caret handoff

## Typical user report

A tester may say that when they move the mouse quickly, the Detail screen feels delayed, the locator cross lags behind, or the cross takes a while to become the text caret again after pointer movement stops.

## Current evidence

Earlier human testing did report visible lag during fast pointer movement and a slow pointer-to-caret handoff. Visual was then refactored so Windows Graphics Capture frame arrival is no longer the interaction clock.

The corrected PMv2 physical regression on 2026-09-24 used two real 1920x1080 displays, independent `GetCursorPos` ground truth, Visual telemetry, and 30 fps full-desktop video. It recorded:

- 793/793 successful Presents;
- zero Present failures;
- zero locator-missing frames during slow horizontal movement;
- zero locator-missing frames during fast horizontal bursts;
- zero locator-missing frames during fast zig-zag movement;
- zero locator-missing frames during edge teleports;
- fast zig-zag pointer evidence selected on 50/50 telemetry frames;
- maximum pointer age during fast zig-zag of about 27.9 ms;
- no genuine rendered-locator disappearance in video/contact-sheet review.

This closes the generic "locator disappears during fast pointer movement" renderer regression for the current build, but it does not prove that every user will perceive motion as perfect in every environment.

## Support response

If a user on the latest alpha still reports that fast mouse movement makes Detail feel delayed or that pointer-to-caret handoff feels slow:

1. do not simply say the issue is fixed;
2. mention that earlier human testing found similar lag, while the corrected physical regression later found zero locator-missing frames and zero Present failures;
3. record the exact Visual version;
4. reproduce on the latest public alpha before drawing a new conclusion;
5. collect current Visual runtime telemetry and, when available, synchronized video evidence;
6. do not recommend GPU/display-driver changes from this symptom alone;
7. if the symptom appears only in Chromium/Edge editing controls, also consider the separate browser caret-compatibility limitation.

---

## Source: installation.md

# Visual alpha installation and update support

## Distribution

Visual alpha uses package id `Standivarius.Visual` on the Velopack `alpha` channel.

Tester flow:

1. download the current Setup executable from the public Visual GitHub prerelease;
2. run Setup;
3. launch Visual from the installed shortcut;
4. expect possible SmartScreen/unknown-publisher warnings while alpha packages remain unsigned.

Current target assumptions are Windows 11, x64, and two active physical displays for the normal two-screen mode.

## Lifecycle status

Published `0.1.0-alpha.1` bypassed Velopack application validation and did not initialize the native lifecycle handler. A physical alpha.1 install showed `Install Partially Succeeded` even though the application could subsequently run.

`0.1.0-alpha.2` integrates Velopack 1.2.0 at the real process entry point and packages the required native runtime as `velopack_libc.dll`.

Local alpha.2 verification on 2026-09-24 proved:

- normal packaging with no `--skipVeloAppCheck`;
- clean Setup exit `0`;
- Setup log reports `Hook executed successfully`;
- installed ProductVersion `0.1.0-alpha.2`;
- installed `Update.exe` and `velopack_libc.dll` are present;
- installed `visual_app.exe --update-check` exits `0` and reaches `result=no_update` against the current public feed;
- consolidated lifecycle verification ends in `LIFECYCLE_RESULT=PASS`.

The exact historical alpha.1 root cause remains formally unproven because its Setup log was not recovered. Missing lifecycle integration is the leading explanation.

## Update maintenance

Engineering commands in installed alpha.2:

- `visual_app.exe --update-check` - check public GitHub prereleases without starting the magnifier UI;
- `visual_app.exe --update-now` - check, download and schedule an available update to apply after Visual exits; no automatic restart is requested yet.

Maintenance diagnostics are appended to `%LOCALAPPDATA%\Standivarius\Visual\logs\visual_update.log`.

Current proof boundary:

- packaging/feed production: proven;
- lifecycle startup: proven locally for alpha.2;
- installed update check: proven locally for alpha.2;
- download/apply: proven end to end by installed public alpha.2 -> public alpha.3 through `--update-now`;
- polished automatic-update UX/policy: not implemented.

Alpha.1 cannot initiate its own update to alpha.2 because it has no update client. The first application-driven proof therefore starts from public alpha.2 and is complete against public alpha.3 through `--update-now`.

## Troubleshooting installation

If Setup fails or reports partial success:

1. record the exact status/error text;
2. preserve the Setup/Velopack log before retrying;
3. confirm the downloaded file is the Visual Setup executable rather than a source archive;
4. do not disable Windows security globally;
5. treat documented unsigned-alpha publisher warnings separately from installer failure;
6. record the installed ProductVersion if files were installed despite the warning;
7. use `visual_diagnostics.exe` or the repository support-bundle procedure when available.

Do not recommend registry edits, policy bypasses, certificate installation or antivirus exclusions unless a reviewed Visual support procedure explicitly requires them.

---

## Source: known-issues.md

# Visual alpha known issues

This document is intentionally conservative. Support should not claim fixes or compatibility that has not been validated.

## VIS-ALPHA-001 — Chromium/Edge editing caret is not reliably validated

**Status:** open compatibility investigation.

Visual has observed valid Microsoft Edge UIA TextPattern caret evidence transiently, but measured automated editing phases have not yet shown reliable semantic-caret availability. Visual may fall back to browser/window focus.

**Support response:** explain that Chromium editing-caret tracking is not yet a validated alpha capability. Do not diagnose the user's Windows accessibility configuration as broken solely from this behavior. Compare with Notepad/native text controls and record the Visual version.

## VIS-ALPHA-002 — fast-pointer locator loss not reproduced in corrected physical harness

**Status:** generic renderer-loss regression closed for the current build; new user-visible reports still require evidence.

Earlier human testing reported visible lag during fast pointer movement and a slow pointer-to-caret handoff. Visual was then refactored so WGC frame arrival is no longer the interaction clock.

The corrected PMv2 physical regression on 2026-09-24 used two real 1920x1080 displays, independent `GetCursorPos` ground truth, Visual telemetry, and 30 fps full-desktop video. It recorded 793/793 successful Presents, zero Present failures, and zero locator-missing frames in slow horizontal, fast horizontal, fast zig-zag, and edge-teleport phases. Fast zig-zag selected pointer evidence on 50/50 frames with maximum pointer age about 27.9 ms. Video/contact-sheet review found no genuine rendered-locator disappearance.

**Support response:** do not simply say every perceived fast-pointer issue is fixed. If a tester on the latest alpha still perceives lag or delayed pointer-to-caret handoff, record the Visual version and capture current runtime telemetry/video evidence. Reproduce on the latest alpha before drawing a conclusion. Do not recommend GPU/display-driver changes from this symptom alone.

## VIS-ALPHA-003 — display topology change intentionally stops Visual

**Status:** current recovery policy by design.

When Windows reports an active display-topology change, the alpha exits rather than continuing with potentially stale monitor/capture assumptions. A physical HDMI disconnect test produced the dedicated topology-change shutdown and Visual relaunched successfully after the desired two-monitor topology returned.

**Support response:** restore the desired Windows display topology, wait for Windows to stabilize, then relaunch Visual. Automatic in-process hot-plug rebind is not currently an alpha feature. Escalate if Visual cannot relaunch while Windows sees the expected displays.

## VIS-ALPHA-004 — unsigned installer warning

**Status:** expected for the current public alpha until trusted code signing is added.

Windows may identify the installer/application as an unknown publisher or show SmartScreen reputation warnings.

**Support response:** verify that the installer came from the official `Standivarius/Visual` GitHub release. Do not tell users to disable security globally. Production/external-testing releases should move to trusted code signing before broad distribution.

## VIS-ALPHA-005 — no polished settings/onboarding/update UI

**Status:** known scope limitation.

Current alpha defaults to the Windows primary display as source and the first different active monitor as Detail destination. Engineering command-line display selection exists, but there is no finished monitor-selection/setup UI. The Velopack update transport is proven, but polished prompts/settings/background update policy are not implemented.

## VIS-ALPHA-006 — source-to-Detail pointer boundary can change tracking target

**Status:** understood current policy; UX decision remains open.

When the native pointer leaves the source monitor and enters the Detail monitor, Visual continues observing pointer movement timing but intentionally stops submitting that pointer as a source POI. Arbitration can immediately fall back to semantic focus/caret, which can make the Detail viewport jump. Pointer POI resumes essentially immediately when the pointer returns to the source monitor.

**Support response:** explain this as current source-monitor POI policy rather than capture/render failure. Do not recommend pointer confinement or driver changes unless a future reviewed product policy explicitly adds them.

---

## Source: product-overview.md

# Visual alpha product overview

## Purpose

Visual is currently a two-monitor low-vision magnification alpha and engineering platform for the future Doxa low-vision workstation.

Current behavior:

- captures the Windows primary/source display using Windows Graphics Capture;
- renders a live magnified Detail view on a second physical monitor using D3D11;
- follows deliberate pointer movement, text caret and keyboard focus using layered evidence;
- uses comfort-margin viewport behavior rather than constant recentering;
- renders a high-contrast locator for the selected target;
- supports global zoom/tracking hotkeys while the source application keeps keyboard focus.

## Current controls

- `Ctrl+Alt+1` — 1x
- `Ctrl+Alt+2` — 2x
- `Ctrl+Alt+4` — 4x
- `Ctrl+Alt+0` — temporary normal view / exact return
- `Ctrl+Alt+T` — tracking on/off
- `Ctrl+Alt+Q` — exit Visual

## Current public alpha status

Current public release: `0.1.0-alpha.3`.

Validated in the current engineering environment:

- WGC/D3D11 two-monitor capture and rendering;
- pointer, semantic caret and keyboard-focus evidence;
- POI arbitration and viewport policy;
- locator rendering;
- exact normal-view return;
- safe shutdown on physical HDMI topology change and successful relaunch after reconnect;
- corrected PMv2 fast-pointer physical regression with zero locator-missing frames and zero Present failures;
- Velopack native lifecycle startup;
- clean installer lifecycle hook;
- installed update check;
- public alpha.2 -> alpha.3 application-driven download/apply update transport;
- normal alpha.3 launch after update and graceful exit.

Important limitations:

- reliable Chromium/Edge editing-caret tracking is not yet established;
- source-to-Detail pointer-boundary behavior still needs an explicit UX policy;
- no polished settings/onboarding/update UI;
- public alpha packages remain unsigned until signing is added;
- no Doxa hardware validation yet.

## Support principle

Visual support should distinguish:

1. Visual runtime/capture problems;
2. Windows display-topology problems;
3. pointer/caret/focus tracking-provider problems;
4. unsupported or not-yet-validated application behavior.

Do not attribute a Windows or hardware problem to Visual without supporting evidence from diagnostics.
