# Visual alpha known issues

This document is intentionally conservative. Support should not claim fixes or compatibility that has not been validated.

## VIS-ALPHA-001 — Chromium/Edge editing caret is not reliably validated

**Status:** open compatibility investigation.

Visual has observed valid Microsoft Edge UIA TextPattern caret evidence transiently, but measured automated editing phases have not yet shown reliable semantic-caret availability. Visual may fall back to browser/window focus.

**Support response:** explain that Chromium editing-caret tracking is not yet a validated alpha capability. Do not diagnose the user's Windows accessibility configuration as broken solely from this behavior.

## VIS-ALPHA-002 — human regression of latest fast-pointer fix pending

**Status:** automated evidence positive; final human confirmation pending.

Earlier human testing found visible lag during fast pointer movement and a slow pointer-to-caret locator handoff. The runtime was subsequently refactored so WGC frame arrival is no longer the interaction clock. Automated stress evidence after the change showed approximately 60 Hz interaction cadence, zero Present failures and pointer-to-semantic-caret handoff around 89 ms.

**Support response:** if a tester on the latest alpha still perceives fast-pointer lag, capture the Visual version and runtime telemetry rather than assuming the old defect is fixed in every environment.

## VIS-ALPHA-003 — display topology change intentionally stops Visual

**Status:** current recovery policy by design.

When Windows reports an active display-topology change, the alpha exits rather than continuing with potentially stale monitor/capture assumptions. A physical HDMI disconnect test produced the dedicated topology-change shutdown and Visual relaunched successfully after the desired two-monitor topology returned.

**Support response:** restore the desired Windows display topology, then relaunch Visual. Automatic in-process hot-plug rebind is not currently a first-alpha feature.

## VIS-ALPHA-004 — unsigned installer warning

**Status:** expected for early infrastructure alpha until trusted code signing is added.

Windows may identify early installer/application builds as an unknown publisher or show SmartScreen reputation warnings.

**Support response:** verify that the installer came from the official `Standivarius/Visual` GitHub release. Do not tell users to disable security globally. Production/external-testing releases should move to trusted code signing before broad distribution.

## VIS-ALPHA-005 — no polished settings/onboarding UI

**Status:** known scope limitation.

Current alpha defaults to the Windows primary display as source and the first different active monitor as Detail destination. Engineering command-line display selection exists, but there is no finished monitor-selection/setup UI.
