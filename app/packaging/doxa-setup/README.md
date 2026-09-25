# Doxa setup surrogate

This directory is deliberately small. It is not a second installer framework.

Until production Doxa hardware is available, the current two-monitor Visual build is the surrogate Doxa software platform:

- Velopack owns packaging/update layout and can produce a per-machine enterprise MSI;
- `visual_diagnostics.exe` supplies structured machine/display evidence;
- `visual_app.exe --health-check` exercises the real WGC/D3D capture-and-present path in the interactive user session;
- `run-surrogate.ps1` combines diagnostics, health, deterministic setup decisions and optional Muse exception planning;
- `support-bundle.ps1` is used automatically when an ambiguous failure occurs while cloud planning is unavailable;
- Muse can select only the read-only diagnostic/escalation actions in `approved-actions.json`.

Run the live two-monitor surrogate:

```powershell
.\app\packaging\doxa-setup\run-surrogate.ps1 -Configuration Release
```

The runner honors a real Windows pending-reboot state. For synthetic walkthroughs, a small JSON `-StateOverride` may override individual state fields without changing the diagnostic executable.

Run the repeatable smoke suite:

```powershell
.\app\packaging\doxa-setup\smoke.ps1 -Configuration Release
```

Include the cloud exception-planner path:

```powershell
.\app\packaging\doxa-setup\smoke.ps1 -Configuration Release -UseMuse
```

The smoke suite validates the live two-monitor health path, a future missing-display state, offline support-bundle fallback and, when requested, an allowlisted Muse decision followed by the real local diagnostic action.

`simulate.ps1` remains the scenario-level decision test. Existing scenario JSON files cover managed deployment, restricted users, App Control, missing display, driver remediation, ambiguous failure, cloud blockage and pending reboot. `expected_display_count` is optional and defaults to two; future Doxa profiles can set the real count without changing the decision logic.

The direct Meta call exists only for repository simulation. A production Doxa client must call a Doxa-owned HTTPS service, which can in turn call Dify/Muse. No model-provider key belongs in the client package.
