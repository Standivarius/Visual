# Visual alpha packaging and support

Visual uses **Velopack 1.2.0** for the Windows alpha installer/update package format.

## Identity

- package id: `Standivarius.Visual`
- friendly name: `Visual Alpha`
- release channel: `alpha`
- default version: defined in `app/version.cmake`
- public repository/releases: `https://github.com/Standivarius/Visual`

The build version can be overridden by CI or locally with `app/build.ps1 -Version <semver>`.

## Ordinary local build

The normal native build does **not** require Velopack or the .NET SDK:

```powershell
.\app\build.ps1 -Configuration Release
```

It builds and tests:

- `visual_app.exe`
- `visual_diagnostics.exe`
- the native production tests

## Local Velopack package

Packaging additionally requires a .NET SDK. The repository contains a local tool manifest in `.config/dotnet-tools.json`, pinned to `vpk` 1.2.0.

```powershell
.\app\packaging\package.ps1 -Version 0.1.0-alpha.1
```

The script builds/tests Visual, stages only the intended shipping files, restores the pinned tool and runs `vpk pack` for the `alpha` channel. Generated staging/release directories are ignored by Git.

To package an already-built matching version:

```powershell
.\app\packaging\package.ps1 -Version 0.1.0-alpha.1 -SkipBuild
```

Do not use `-SkipBuild` with a version that differs from the version embedded in the existing binaries.

## First public bootstrap

The canonical local repository historically contained substantial uncommitted engineering/research work. The first-public-release helper therefore deliberately stages only:

- `.gitignore`
- `.config/`
- `.github/`
- `app/`
- `support/`

It refuses to commit unexpected staged paths.

After authenticating GitHub locally, the scoped first-release operation is:

```powershell
.\app\packaging\bootstrap-public-alpha.ps1
```

It:

1. adds `https://github.com/Standivarius/Visual.git` as `origin` only if no origin exists;
2. verifies the current branch is `master`;
3. stages only the public Visual application/distribution/support surface;
4. commits `chore: bootstrap Visual alpha distribution`;
5. pushes `master`;
6. creates and pushes `v0.1.0-alpha.1`;
7. the pushed tag triggers GitHub Actions.

Do not run the bootstrap script from another repository or with unrelated files already staged.

## GitHub alpha releases

`.github/workflows/release-alpha.yml` triggers only from pushed tags matching:

`v*-alpha.*`

For example:

`v0.1.0-alpha.1`

The Windows runner:

1. validates the alpha tag/version;
2. installs a .NET 8 SDK for the pinned Velopack CLI;
3. builds and runs CTest with that version embedded in the binaries;
4. restores the pinned Velopack CLI;
5. attempts to download previous public alpha release data so Velopack can produce delta assets when possible;
6. packages the alpha installer/update assets;
7. publishes a GitHub **pre-release** using the repository `GITHUB_TOKEN`.

No local GitHub CLI or local Velopack installation is required for the CI publication path.

If organization/repository policy limits `GITHUB_TOKEN` to read-only access, enable **Settings → Actions → General → Workflow permissions → Read and write permissions** for the repository before retrying the release workflow.

## Signing status

The first infrastructure releases are **unsigned development alphas**. Windows may display SmartScreen or unknown-publisher warnings. Do not treat those warnings as an installer failure.

Before inviting non-technical external testers, add trusted Windows code signing to the release workflow.

## Structured diagnostics

Run:

```powershell
.\app\build\Release\visual_diagnostics.exe
```

or write JSON to a file:

```powershell
.\app\build\Release\visual_diagnostics.exe --out .\diagnostics.json
```

The current schema reports:

- Visual version/package/channel;
- Windows version/build;
- process architecture;
- monitor geometry/mode/DPI;
- GPU adapter names;
- adjacent Visual-executable presence/size.

It intentionally does **not** report the installed path, because a per-user path can reveal the Windows account name. It also does not collect screenshots, document/window contents, usernames, arbitrary file listings, environment variables, passwords or tokens.

## Support bundle

From a development checkout:

```powershell
.\app\packaging\support-bundle.ps1
```

This creates a timestamped ZIP containing `diagnostics.json` and a privacy note.

Runtime telemetry is opt-in for a bundle. Include it only when explicitly requested:

```powershell
.\app\packaging\support-bundle.ps1 -TelemetryPath C:\path\to\visual_app_telemetry.csv
```

The script accepts only a CSV with the expected Visual telemetry header and imposes a 100 MB size limit before copying it verbatim into the bundle.

This deterministic bundle is intended to become the input to Dify/LLM-assisted support. The LLM should interpret structured evidence and reviewed documentation; it should not receive unrestricted administrator or shell authority.

## Dify prototype

Version-controlled support knowledge and the initial Dify system prompt live under:

- `support/knowledge/`
- `support/dify/`

For the first prototype use Dify Cloud rather than operating a VPS. Create one knowledge base from `support/knowledge/`, attach it to a support Chatbot/Chatflow and use `support/dify/system-prompt.md` as the assistant instruction set.

No Dify or LLM API key belongs in this repository or inside `Visual.exe`.

## Update integration status

Velopack packaging and the public release feed are established by this layer. **In-app check/download/apply is a separate integration step** using Velopack's C/C++ library and should be validated with an `alpha.1 -> alpha.2` update before we call automatic updating proven.
