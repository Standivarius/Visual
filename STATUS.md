# Visual / Doxa project status

**Date:** 2026-09-24
**Canonical machine:** `MARIUS-DELL`
**Canonical workspace:** `C:\dev\Visual`
**Public repo:** `https://github.com/Standivarius/Visual`

## Product direction

Visual is the current Windows magnification/tracking component and two-monitor engineering test bed for Doxa. Doxa research should evaluate magnification/access, productivity/context and ergonomics together rather than assuming more active displays are automatically better.

## Current Visual state

Visual is a functional two-monitor alpha with validated WGC capture, cached source texture, D3D11 Detail rendering, independent interaction/render cadence, pointer/UIA caret/keyboard-focus evidence, POI arbitration, viewport hold/pan/jump, locators, global zoom/tracking controls, exact normal/return, non-activating Detail, topology-invalidating shutdown/relaunch and native production tests.

Hotkeys:

- `Ctrl+Alt+1` -> 1x
- `Ctrl+Alt+2` -> 2x
- `Ctrl+Alt+4` -> 4x
- `Ctrl+Alt+0` -> normal/return
- `Ctrl+Alt+T` -> tracking toggle
- `Ctrl+Alt+Q` -> exit

Browser/Chromium semantic-caret support remains unvalidated.

## Pointer regression - closed for current build

Canonical crossing run:

`lab/benchmarks/visual_regression/runs/2026-09-24_10-40-18__pointer_crossing`

Key evidence:

- two physical 1920x1080 displays;
- PMv2-aware harness;
- 948/948 successful Presents;
- 659 independent cursor samples;
- flushed lifecycle markers including `first_source_frame` and `first_render_frame`;
- graceful Visual exit.

The source-to-Detail boundary behavior is current POI policy, not capture/render failure. Once the pointer leaves the source monitor, pointer evidence is filtered from the candidate set and semantic focus can take over, causing the Detail viewport to jump. Pointer POI resumes immediately when the native cursor returns to the source. This remains a UX-policy decision.

Canonical fast-visibility run:

`lab/benchmarks/visual_regression/runs/2026-09-24_11-04-26__pointer_visibility`

Results:

- 793/793 successful Presents;
- 561 independent cursor samples;
- zero Present failures;
- zero locator-missing frames in slow horizontal, fast horizontal, fast zig-zag and edge-teleport phases;
- video/contact-sheet review found no genuine rendered-locator disappearance;
- six telemetry anomaly rows were short timing/source-selection artifacts, not renderer loss.

Do not spend further cycles on generic pointer-loss reruns unless tracking/rendering changes or a new user-visible reproduction appears.

## Telemetry/harness changes

`app/src/main.cpp` now flushes the telemetry header and first numeric row immediately and emits flushed lifecycle markers:

- `telemetry_started`
- `capture_started`
- `first_source_frame`
- `first_render_frame`
- `capture_failure`

The physical harnesses establish PMv2 awareness before display enumeration, require a successful rendered telemetry row, detect early exit/readiness failure, and request graceful shutdown before force-kill fallback.

## Velopack lifecycle and public update milestone complete

Public prereleases `v0.1.0-alpha.2` and `v0.1.0-alpha.3` are published. Alpha.2 contains the lifecycle/update transport implementation; alpha.3 is a version-only transport-validation target used to prove the application-driven update path end to end.

Alpha.1 history:

- was packaged with `--skipVeloAppCheck true`;
- did not initialize the native Velopack lifecycle handler;
- physical installation showed `Install Partially Succeeded` although Visual could run;
- the historical alpha.1 Setup log was not recovered, so missing lifecycle integration is the leading explanation rather than a conclusively proven historical root cause.

### Current alpha.2 implementation

- pinned Velopack C/C++ SDK `1.2.0`;
- pinned native SDK ZIP SHA-256 `547262ed7a1ab1ff62f580aa53851ede2f1a451ac61b8974eb7bc01117488835`;
- pinned .NET SDK `8.0.425` for the CLI;
- pinned `vpk 1.2.0` tool manifest;
- `app/src/velopack_entry.cpp` owns real `wWinMain`;
- `Velopack::VelopackApp::Build().Run()` executes before normal Visual initialization;
- normal app implementation remains in `main.cpp` as renamed `VisualProductMain`;
- explicit `--update-check` and `--update-now` maintenance commands execute before normal magnifier startup;
- packaging no longer uses `--skipVeloAppCheck`.

Velopack SDK archive files remain:

- `lib/velopack_libc_win_x64_msvc.dll`
- `lib/velopack_libc_win_x64_msvc.dll.lib`

The import library encodes the runtime basename `velopack_libc.dll`; CMake copies/renames the archive DLL to that name beside `visual_app.exe`, and the package ships `lib/app/velopack_libc.dll`.

### Proven local alpha.2 result

Final candidate build/package result on MARIUS-DELL:

- Release build: success;
- CTest: 5/5 passed;
- Visual ProductVersion: `0.1.0-alpha.2`;
- Visual PE machine: x64 (`0x8664`);
- native Velopack PE machine: x64 (`0x8664`);
- normal `vpk 1.2.0` package: success with lifecycle validation enabled;
- full package: `Standivarius.Visual-0.1.0-alpha.2-alpha-full.nupkg`;
- Setup: `Standivarius.Visual-alpha-Setup.exe`;
- package payload contains `lib/app/velopack_libc.dll`.

A first alpha.2 package installed successfully at the Setup level but its lifecycle hook/update check returned `0xC0000135` because the runtime DLL had been staged under the SDK archive filename. `dumpbin /dependents` proved `visual_app.exe` imports `velopack_libc.dll`. Copying the same SDK DLL under that imported name immediately made installed `--update-check` work. CMake/package/verifiers were then corrected.

Clean corrected reinstall proof:

- uninstall exit: `0`;
- corrected Setup exit: `0`;
- Setup log: `Hook executed successfully`;
- Setup log: `Installation completed successfully!`;
- installed version: `0.1.0-alpha.2`;
- installed `velopack_libc.dll`: present;
- legacy wrong runtime filename: absent;
- installed `Update.exe`: present;
- installed `--update-check` exit: `0`;
- log: `installed_version=0.1.0-alpha.2`;
- log: `result=no_update`.

The hardened verifier was rerun against the corrected installation and reported:

- `setup_exit_code=0`;
- `hook_failure_lines=0`;
- `hook_success_lines=1`;
- `installed_update_check_exit_code=0`;
- `LIFECYCLE_RESULT=PASS`.

Canonical evidence:

- `app/packaging/alpha2-candidate-verification.log`;
- `artifacts/velopack_0.1.0-alpha.2_lifecycle_verification.log`;
- `artifacts/velopack_0.1.0-alpha.2_setup.log`;
- `artifacts/velopack_0.1.0-alpha.2_setup_corrected.log`;
- `%LOCALAPPDATA%\Standivarius.Visual\visual_update.log`.

## Update claim boundaries

1. package/feed production - proven;
2. native lifecycle startup - proven locally for alpha.2;
3. installed update-check transport - proven locally for alpha.2;
4. download/apply transport - proven end to end: installed public alpha.2 detected/downloaded/applied public alpha.3 through `--update-now`;
5. polished automatic-update UX/policy - not implemented.

Alpha.1 cannot initiate its own alpha.1 -> alpha.2 update because it contains no update client. The first application-driven update proof therefore starts from public alpha.2 and is now complete against public alpha.3.

## Release state

Public release chain:

- `v0.1.0-alpha.2` -> release commit `7215af2`; GitHub Actions release completed successfully;
- `v0.1.0-alpha.3` -> validation commit `712cba2`; GitHub Actions release completed successfully;
- installed public alpha.2 detected alpha.3 (`exit 10`, `available_version=0.1.0-alpha.3`), downloaded it, scheduled apply, and the installed ProductVersion became `0.1.0-alpha.3`;
- updated alpha.3 then returned `result=no_update` and passed a normal launch + graceful `Ctrl+Alt+Q` exit (`0`).

Canonical update proof:

`artifacts/2026-09-24__alpha2_to_alpha3_update_proof.md`

The worktree still contains substantial unrelated modified/untracked bridge, Doxa research, harness and artifact work. Those remain intentionally outside the release commits.

## Immediate priority

The packaging/lifecycle/update-transport milestone is complete. Next engineering priorities are product work rather than more release plumbing:

1. decide and implement explicit source-to-Detail pointer-boundary UX policy;
2. validate/fix Chromium/Edge editing-caret behavior;
3. resume Context+Detail productivity experiments;
4. continue Dify pilot support work as appropriate;
5. design polished update UX/policy (prompts, settings, restart timing/background checks) on top of the now-proven transport.
