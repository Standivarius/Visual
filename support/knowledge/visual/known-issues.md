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
