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
