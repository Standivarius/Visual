# Visual release reproducibility and dependency controls

Visual's release path is **dependency-pinned and clean-room verifiable**, but it is not yet a byte-for-byte hermetic build. Downloaded release inputs are content-pinned. The Windows host toolchain (MSVC, CMake and Windows SDK) is validated/recorded in provenance and remains the next reproducibility boundary to tighten.

## Source of truth

`release-dependencies.lock.json` is the tracked lock for downloaded build/package inputs. It currently pins:

- Velopack native C/C++ SDK `1.2.0` by SHA-256;
- .NET SDK `8.0.425` win-x64 archive by SHA-512;
- Velopack CLI NuGet package `vpk 1.2.0` by SHA-256.

Do not duplicate these hashes in new release scripts. Add/change a dependency in the lock and update the relevant acquisition script together.

## Cache preparation

Online preparation downloads only missing locked inputs and verifies their digests:

```powershell
.\app\packaging\ensure-release-dependencies.ps1
```

Offline verification performs no dependency download and fails if a cache file is missing or wrong:

```powershell
.\app\packaging\ensure-release-dependencies.ps1 -Offline
```

The caches are ignored by Git. The Velopack CLI restore uses a generated NuGet configuration containing only the verified repo-local NuGet folder; it does not fall back to the user's global NuGet cache or nuget.org during packaging.

## Secret/environment build input

`DOXA_CLOUD_CLIENT_TOKEN` is not a downloadable dependency and is not stored in the lock file. The build reads it only from the **current process environment**; `build.ps1` does not silently import the builder's User-scope environment anymore. GitHub release CI maps it explicitly from the `DOXA_CLOUD_CLIENT_TOKEN` repository secret and fails if it is absent.

Provenance inspects the generated build configuration and records only whether a client token was embedded, never its value or hash. The current token is an expendable lab/client bearer token and is embedded into the Windows client when supplied, so it must not be treated as a strong production secret. Production cloud authentication remains a separate hardening task.
## Offline-dependency release package

```powershell
.\app\packaging\package.ps1 `
  -Version 0.1.0-alpha.N `
  -Configuration Release `
  -OutputDir C:\path\to\release `
  -OfflineDependencies
```

`-OfflineDependencies` means downloaded build/package dependencies must already exist in the verified caches. Velopack's own CLI version check is also disabled during packaging. Network access for release publication/update-feed operations is a separate, explicit stage.

Every package run gates on `audit-build-inputs.ps1` and emits:

- `Standivarius.Visual-<version>.build-inputs.json`;
- `Standivarius.Visual-<version>.provenance.json`;
- `Standivarius.Visual-<version>.sbom.cdx.json` (CycloneDX 1.6).

The provenance records Git commit/dirty state, dependency-lock digest, exact host toolchain versions and release artifact hashes.

## Observed build-input audit

`audit-build-inputs.ps1` reads MSBuild `*.read.*.tlog` traces produced by the compiler/linker/resource/custom-build steps. Repo inputs must be one of:

- Git-tracked source/configuration;
- generated files under the active build tree;
- the pinned Velopack native cache.

External inputs must be under the active Visual Studio installation, Windows SDK, or Windows directory. Release packaging fails on an unexpected input.

This is intentionally narrower than mkcheck2/eBPF: it audits MSBuild-observed build reads, not every process-level filesystem or network access. It is useful for catching accidental local files and unexpected compiler/link inputs on Windows without claiming complete dependency-graph validation.

Manual audit:

```powershell
.\app\packaging\audit-build-inputs.ps1 -FailOnUnexpected
```

## Detached clean-room release

After the release scripts are committed, build from a detached worktree containing only tracked source plus verified locked caches:

```powershell
.\app\packaging\clean-room-release.ps1 `
  -Version 0.1.0-alpha.N `
  -Commit HEAD `
  -OutputDir C:\path\to\clean-release `
  -RequireCloudToken
```

The driver:

1. verifies the canonical dependency caches offline;
2. creates a detached worktree at the requested commit;
3. copies only the locked cache files into that worktree;
4. packages with `-OfflineDependencies`;
5. requires the tracked worktree to remain clean;
6. verifies provenance points to the requested commit and reports no tracked dirtiness.

This is the preferred local release-candidate proof on MARIUS-DELL.

## CI boundary

`.github/workflows/release-alpha.yml` first downloads and hash-verifies the locked inputs, then builds/packages in offline dependency mode. Network use after that is explicit and limited to release-feed metadata and GitHub publication.

## What remains

For stronger/byte-reproducible releases we still need to pin the Windows host build environment itself (for example a controlled VM/image containing exact MSVC, CMake and Windows SDK builds), add trusted code signing, and decide how signing timestamps are represented in reproducibility comparisons.
