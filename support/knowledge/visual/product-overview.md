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

## Current alpha status

Validated in the current engineering environment:

- WGC/D3D11 two-monitor capture and rendering;
- pointer, semantic caret and keyboard-focus evidence;
- POI arbitration and viewport policy;
- locator rendering;
- exact normal-view return;
- safe shutdown on physical HDMI topology change and successful relaunch after reconnect;
- automated fast-pointer cadence stress with zero Present failures.

Important limitations:

- human regression after the latest fast-pointer/caret cadence fix is still pending;
- reliable Chromium/Edge editing-caret tracking is not yet established;
- no polished settings UI;
- no Doxa hardware validation yet;
- current public alpha packaging is unsigned until signing is added.

## Support principle

Visual support should distinguish:

1. Visual runtime/capture problems;
2. Windows display-topology problems;
3. pointer/caret/focus tracking-provider problems;
4. unsupported or not-yet-validated application behavior.

Do not attribute a Windows or hardware problem to Visual without supporting evidence from diagnostics.
