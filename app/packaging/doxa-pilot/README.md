# Doxa two-monitor pilot

This is the portable pre-Doxa-hardware end-to-end test kit.

The kit contains the normal Velopack Visual installer plus a thin pilot orchestrator. On a Windows PC with two extended monitors it can:

- install or reuse the requested Visual version;
- run `visual_diagnostics.exe`;
- exercise the real bounded WGC/D3D capture-and-present health check;
- apply the deterministic Doxa setup policy;
- send only structured setup state to a Doxa HTTPS planner endpoint when the local checks cannot explain a failure;
- validate the returned action against `approved-actions.json` before doing anything locally;
- run the approved diagnostic follow-up and record evidence;
- safely exercise the cloud path on an otherwise healthy PC by injecting an ambiguous health failure while retaining the real passing health result.

`build-pilot-kit.ps1` generates an unpacked kit and ZIP under `artifacts/`. The generated `pilot-config.json` contains only the temporary planner URL and an expendable lab token. It must not contain a Dify or Meta provider key and should not be committed.

`reset-pilot.ps1` uses Visual's registered silent Velopack uninstall first, then removes only Visual/Doxa-owned pilot state and exact leftover shortcuts/registry entries. It deliberately leaves shared Microsoft runtimes, GPU drivers and vendor display drivers untouched.

For the lab test, double-click `INSTALL_AND_TEST.cmd`. After the test, double-click `RESET_TEST_MACHINE.cmd` to return the machine to a clean Visual/Doxa test state.
