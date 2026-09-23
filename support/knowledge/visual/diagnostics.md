# Visual structured diagnostics

## Purpose

`visual_diagnostics.exe` produces a small machine-readable JSON report for support. It is intended to provide enough technical context to distinguish Visual, Windows-display and hardware/environment problems without collecting user content.

## Current schema

The initial report includes:

- `schema_version`;
- `app_id`;
- `visual_version`;
- `release_channel`;
- generation timestamp in UTC;
- process architecture;
- Windows major/minor/build;
- whether an adjacent `visual_app.exe` exists and its file size;
- active monitor count;
- per-monitor device name, bounds, display mode, refresh rate and effective DPI/scaling;
- GPU adapter names.

The installed filesystem path is intentionally not reported because a per-user path can expose the Windows account name.

## Privacy boundary

The diagnostic executable intentionally does not collect:

- screenshots;
- window/document/browser contents;
- usernames;
- arbitrary file listings;
- environment-variable dumps;
- passwords, tokens or API keys.

A support bundle contains `diagnostics.json` plus a privacy note. Runtime CSV telemetry is included only if an explicit telemetry path is supplied by the user/support workflow.

## Interpretation principles

- `active_monitor_count < 2` means the normal two-screen Visual scenario is not currently available, but does not by itself identify the cause.
- Monitor geometry/DPI should be interpreted as Windows currently reports it; do not infer physical placement beyond reported bounds.
- GPU adapter presence does not prove WGC or D3D rendering health.
- The initial diagnostics schema is primarily static/environmental. Runtime health/error codes should be added as the field-support requirements become clearer.

## Support bundle command from a development checkout

```powershell
.\app\packaging\support-bundle.ps1
```

Optional explicit telemetry inclusion:

```powershell
.\app\packaging\support-bundle.ps1 -TelemetryPath C:\path\to\visual_app_telemetry.csv
```

When Visual becomes a polished installed product, expose this capability through a user-facing support action rather than requiring a PowerShell command.
