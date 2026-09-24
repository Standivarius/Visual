# Visual alpha packaging and support

Visual uses Velopack 1.2.0 for Windows alpha packaging, native lifecycle startup and controlled update-maintenance plumbing.

## Identity

- package id: `Standivarius.Visual`
- friendly name: `Visual Alpha`
- channel: `alpha`
- canonical default version: `0.1.0-alpha.3`
- public repository: `https://github.com/Standivarius/Visual`

## Pinned toolchain

The release path intentionally pins both halves of Velopack integration:

- Velopack CLI (`vpk`): `1.2.0`, via `.config/dotnet-tools.json`;
- Velopack C/C++ SDK: `1.2.0`;
- Velopack SDK ZIP SHA-256: `547262ed7a1ab1ff62f580aa53851ede2f1a451ac61b8974eb7bc01117488835`;
- .NET SDK for the CLI: `8.0.425`;
- .NET SDK win-x64 ZIP SHA-512: `f0b6f15bf6f1a0507205c0cb102ab99e1dee875c4682c8ed94665be1d580186a06b21455e83b3a01a0ff7f4cd887b67420f2e2fe09ed985534a4cea488ae1af9`.

`app/packaging/get-velopack-sdk.ps1` acquires/validates the native SDK into ignored `app/third_party/velopack/1.2.0/`.

`app/packaging/get-dotnet-sdk.ps1` can acquire the pinned .NET SDK into ignored `tools/dotnet/8.0.425/` when the exact SDK is not already installed. Packaging selects the pinned version rather than accepting an arbitrary global SDK.

## Native lifecycle integration

`app/src/velopack_entry.cpp` owns the real application entry point. It executes `Velopack::VelopackApp::Build().Run()` before normal Visual initialization, handles explicit update-maintenance commands, then delegates normal launches to `VisualProductMain` in `main.cpp`.

The SDK file is named `velopack_libc_win_x64_msvc.dll`, while the MSVC import library records the runtime dependency name `velopack_libc.dll`. CMake therefore copies the SDK DLL to `velopack_libc.dll`, which is the name shipped in the package.

Packaging deliberately does not use `--skipVeloAppCheck`.

## Build

```powershell
.\app\build.ps1 -Configuration Release
```

The build acquires the pinned native SDK when necessary, builds `visual_app.exe` and `visual_diagnostics.exe`, copies `velopack_libc.dll`, and runs CTest.

## Package

```powershell
.\app\packaging\package.ps1 -Version 0.1.0-alpha.2
```

The script builds/tests Visual, stages only intended shipping files, restores the pinned `vpk`, and runs normal `vpk --yes pack` for the alpha channel. `--yes` keeps repeated local/CI output-directory use noninteractive; it does not bypass application validation.

To package an already-built matching version:

```powershell
.\app\packaging\package.ps1 -Version 0.1.0-alpha.2 -SkipBuild
```

Do not use `-SkipBuild` for a version different from the binaries already built.

## Enterprise MSI

For enterprise deployment, reuse Velopack's MSI generation instead of maintaining a second installer project:

```powershell
.\app\packaging\package.ps1 -Version 0.1.0-alpha.3 -EnterpriseMsi
```

`-EnterpriseMsi` adds Velopack `--msi --instLocation PerMachine`, producing a machine-wide MSI suitable for SYSTEM-context deployment through Intune/Configuration Manager. The MSI can be deployed directly. Organizations that standardize on PSAppDeployToolkit can wrap the same MSI using PSADT rather than a Doxa-specific enterprise wrapper.

A 2026-09-25 local packaging proof produced `Standivarius.Visual-alpha.msi`; Windows Installer metadata reported `ALLUSERS=1`, confirming per-machine scope.

## Verification

Candidate verification:

```powershell
.\app\packaging\verify-alpha2-candidate.ps1 -Version 0.1.0-alpha.2
```

It verifies:

- Release build and CTest;
- ProductVersion;
- x64 PE machine type for Visual and the native Velopack runtime;
- normal `vpk` packaging with no lifecycle-validation bypass;
- Setup/full-package/feed output;
- feed contains the requested version;
- Velopack SDK archive SHA-256.

Lifecycle/install verification:

```powershell
.\app\packaging\verify-alpha2-install.ps1 -Version 0.1.0-alpha.2
```

It verifies Setup exit status, installed version/runtime/`Update.exe`, successful lifecycle-hook log evidence, and the installed `--update-check` path. It now explicitly fails on a non-zero Velopack hook or partial-install marker even if Setup itself returns zero.

### Proven local alpha.2 result - 2026-09-24

On MARIUS-DELL:

- Release build: success;
- CTest: 5/5 passed;
- `vpk 1.2.0` package: success without `--skipVeloAppCheck`;
- full package: `Standivarius.Visual-0.1.0-alpha.2-alpha-full.nupkg`;
- Setup: `Standivarius.Visual-alpha-Setup.exe`;
- package payload contains `lib/app/velopack_libc.dll`;
- clean Setup exit: `0`;
- install hook: `Hook executed successfully`;
- installed ProductVersion: `0.1.0-alpha.2`;
- installed update check exit: `0`;
- installed update result: `no_update`;
- consolidated verifier: `LIFECYCLE_RESULT=PASS`.

The earlier alpha.2 candidate failure was traced to shipping the SDK archive filename (`velopack_libc_win_x64_msvc.dll`) instead of the DLL basename encoded by the import library (`velopack_libc.dll`). That defect is corrected and covered by package/install verification.

## Explicit update maintenance

Installed engineering commands:

```powershell
visual_app.exe --update-check
visual_app.exe --update-now
```

`--update-check` checks the public GitHub prerelease feed without launching the magnifier. `--update-now` checks, downloads and schedules an available update for apply after process exit, with no automatic restart.

Current claim boundaries:

1. package/feed production - proven;
2. native Velopack lifecycle startup - proven locally for alpha.2;
3. installed update-check transport - proven locally for alpha.2;
4. download/apply transport - proven end to end by installed public alpha.2 -> public alpha.3 using `--update-now`;
5. user-facing automatic-update UX/policy - not implemented.

Published alpha.1 cannot initiate alpha.1 -> alpha.2 itself because alpha.1 contains no update client. The first application-driven proof therefore starts from public alpha.2 and is complete against public alpha.3 through `--update-now`.


## Public alpha.2 -> alpha.3 update proof

The application-driven update transport is now proven end to end against public releases:

- installed public alpha.2 `--update-check` exited `10` and logged `available_version=0.1.0-alpha.3`;
- installed public alpha.2 `--update-now` logged `download_start`, `download_complete`, then `result=apply_scheduled`;
- Velopack replaced the installed tree and ProductVersion became `0.1.0-alpha.3`;
- updated alpha.3 `--update-check` exited `0` with `result=no_update`;
- updated alpha.3 launched normally and exited gracefully via `Ctrl+Alt+Q` with exit code `0`.

The remaining update work is UX/policy, not transport plumbing.
## GitHub alpha release workflow

`.github/workflows/release-alpha.yml` runs for tags matching `v*-alpha.*`.

The workflow:

1. validates the tag/version;
2. installs exact .NET SDK `8.0.425`;
3. builds/tests with the tag version embedded;
4. restores pinned `vpk 1.2.0`;
5. downloads prior alpha release metadata when available;
6. packages with normal Velopack validation;
7. publishes a GitHub prerelease.

Do not rewrite an existing alpha tag. `v0.1.0-alpha.1` remains immutable; alpha.2 is a new tag/release.

## alpha.1 history

Published `0.1.0-alpha.1` was packaged with `--skipVeloAppCheck true` and did not initialize the native lifecycle handler. A physical alpha.1 installation showed `Install Partially Succeeded`, although Visual could run afterward. The missing lifecycle integration is the leading explanation, but the historical alpha.1 Setup log was not recovered, so that historical root cause is not claimed as conclusively proven.

Alpha.2 local testing now shows a normal `Installation Succeeded` result and a successful lifecycle hook.

## Signing

Current alpha packages are unsigned. SmartScreen/unknown-publisher warnings are expected until trusted Windows code signing is added.

## Diagnostics and support

`visual_diagnostics.exe` produces privacy-limited structured diagnostics. `app/packaging/support-bundle.ps1` creates a support ZIP; runtime telemetry is included only when explicitly provided.

Support knowledge lives under `support/knowledge/`. Dify/LLM support remains advisory and must not receive unrestricted administrator authority or embedded secrets.
