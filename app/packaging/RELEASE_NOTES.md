# Visual 0.1.0-alpha.2

Visual is an early two-monitor engineering alpha for low-vision magnification research and Doxa development.

## Changes in alpha.2

- adds native Velopack 1.2.0 lifecycle startup at the real Windows process entry point;
- packages the native Velopack runtime under the imported name `velopack_libc.dll`;
- removes the previous `--skipVeloAppCheck` packaging bypass;
- adds explicit installed-app `--update-check` and `--update-now` engineering commands;
- hardens telemetry startup/capture markers so failed physical regression runs preserve useful evidence;
- retains GPU magnification, pointer/caret/focus tracking, viewport following, target locator, zoom hotkeys and exact normal-view return.

Local release verification completed successfully on 2026-09-24: Release build, CTest 5/5, normal Velopack packaging, clean Setup lifecycle hook, installed alpha.2 ProductVersion and installed GitHub update check all passed.

## Known limitations

- packages are unsigned development alphas;
- reliable browser editing-caret behavior is not yet validated;
- product settings/onboarding/update UI are unfinished;
- source-to-Detail cursor-boundary behavior still needs an explicit UX policy;
- the `--update-now` download/apply path requires a later published alpha for end-to-end proof.
