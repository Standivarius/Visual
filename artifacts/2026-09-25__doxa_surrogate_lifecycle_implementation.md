# Doxa surrogate lifecycle implementation

**Date:** 2026-09-25

## Implemented

The existing two-monitor Visual build now acts as the surrogate Doxa software platform above the physical-hardware layer.

- `visual_app.exe --health-check` runs the real WGC/D3D capture-and-present path in the interactive session and succeeds only after both a source frame and a successful render/present are observed. Timeout exit: `30`.
- `app/packaging/doxa-setup/run-surrogate.ps1` combines live diagnostics, a bounded graphics health check, deterministic setup decisions, optional state overrides for future Doxa hardware simulation, optional Muse exception planning, approved local follow-up diagnostics, and offline support-bundle fallback.
- `app/packaging/doxa-setup/smoke.ps1` provides a repeatable surrogate lifecycle test.
- setup decisions now support a configurable expected display count; existing scenarios still default to two displays.
- enterprise update policy is implemented. `VISUAL_UPDATE_MODE=it-managed` or `HKLM\SOFTWARE\Standivarius\Visual\UpdateMode=ITManaged` prevents `--update-check` and `--update-now` from contacting/using the update service and returns exit `43`.
- the existing Velopack per-machine MSI path remains the enterprise package; no second installer framework was added.

## Validation

Release build: PASS.

CTest: 5/5 PASS.

Normal Visual lifecycle regression: PASS. Synthetic `WM_DISPLAYCHANGE` produced exit `12`, topology-shutdown telemetry and 14 rendered frames before shutdown.

Surrogate smoke with Muse: 5/5 PASS:

1. live two-monitor WGC/D3D health -> `installed_ok`;
2. future Doxa missing-display simulation -> `waiting_for_display`;
3. ambiguous failure with cloud blocked -> local support bundle created;
4. ambiguous failure with Muse -> `run_minimal_capture_probe`, followed by the real local health probe -> PASS;
5. IT-managed update policy -> exit `43`.

Enterprise packaging: PASS. Velopack 1.2.0 produced the per-machine MSI with `ALLUSERS=1`; the staged packaged executable passed `--health-check` with exit `0`.

All nine existing deterministic Doxa setup scenarios still pass.

## Observed environment condition

MARIUS-DELL currently reports a pending Windows reboot. The live setup policy therefore returns `reboot_required` unless a synthetic walkthrough explicitly overrides that field. The implementation does not hide or ignore the real machine state.

## Boundaries

The physical SM770 Doxa adapter is intentionally not implemented yet because the unit is not present. No USB IDs, controller state, firmware version or three-screen behavior are fabricated.

The current direct Muse call is still a repository/lab simulation. Production remains Doxa client -> Doxa-owned HTTPS service -> Dify/Muse.

Current alpha packaging is still unsigned; enterprise code signing remains separate work.
