# Troubleshooting: Detail does not follow the expected pointer/caret/focus target

## Symptom

The magnified Detail view follows the wrong target, follows late, remains on the pointer after typing resumes, or follows browser/window focus instead of a text caret.

## Current tracking model

Visual combines several evidence sources:

- recently moving pointer;
- UIA TextPattern2 caret;
- UIA TextPattern insertion range;
- Win32 caret fallback;
- UIA keyboard focus;
- Win32 focus fallback.

Recent deliberate pointer movement temporarily outranks caret/focus. When pointer evidence becomes stale, valid semantic caret/focus evidence should resume.

## Safe checks

1. Record the Visual version.
2. Identify the source application (for example Notepad or Edge) and the exact control being edited.
3. Test ordinary caret navigation in Notepad. If that works, the problem may be application/provider-specific rather than a general Visual tracking failure.
4. If available, collect Visual runtime telemetry covering the failure and include it explicitly in a support bundle.
5. Check the known-issues document before recommending repair.

## Browser caveat

Reliable Chromium/Edge editing-caret tracking is not yet validated. Browser focus fallback can therefore be expected in some alpha scenarios and should not automatically be treated as a Windows/UIA installation fault.

## Escalate when

- the problem reproduces in Notepad/native text controls on the current alpha;
- pointer tracking visibly stalls despite continued movement;
- the same target is repeatedly wrong across multiple applications;
- telemetry shows unexpected long evidence ages or render stalls.

Do not recommend driver/firmware changes for a tracking-provider problem unless separate diagnostics establish a hardware/display failure.
