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
