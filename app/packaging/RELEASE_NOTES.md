# Visual 0.1.0-alpha.5

Visual 0.1.0-alpha.5 is the first normal-install Doxa cloud-assistance pilot.

A standard Velopack installation now performs the Doxa setup check automatically on the first installed launch. It verifies the two-display Windows baseline, runs the real bounded WGC/D3D graphics health check, sends only structured setup state to the permanent Doxa Cloudflare Worker, and follows only approved diagnostic/escalation actions. The Worker keeps the Dify Service API key server-side and calls the published `Doxa Installer Planner` app, which uses Muse Spark 1.3 Contributor for ambiguous failures.

Healthy installations complete deterministically after the cloud connection is verified; Dify/Muse is invoked only when the deterministic checks leave an ambiguous problem. Setup state is retained under Local AppData so a failed first run can be retried by starting Visual again.

The physical Silicon Motion/Doxa hardware layer remains simulated until the production unit is available.

# Visual 0.1.0-alpha.4

Visual 0.1.0-alpha.4 is the first portable two-monitor Doxa lifecycle pilot.

It adds a bounded interactive graphics health check, a portable install-and-test kit, cloud exception-planner transport with a strict action allowlist, a safe injected-failure exercise for proving the AI round trip on healthy hardware, and a repeatable full test reset/uninstall path. It also carries the enterprise MSI and IT-managed update policy developed after alpha.3.

The physical Silicon Motion/Doxa adapter is intentionally still simulated; no SM770 state is fabricated before production hardware is available.

# Visual 0.1.0-alpha.3

Visual 0.1.0-alpha.3 is a focused update-transport validation prerelease.

There are no intended magnifier-behavior changes relative to alpha.2. This release exists to provide a genuine newer public alpha target so an installed alpha.2 can exercise the native Velopack `--update-check` and `--update-now` path end to end.

It retains the alpha.2 lifecycle/runtime fixes:

- native Velopack 1.2.0 lifecycle startup at the real Windows process entry point;
- correctly packaged `velopack_libc.dll` runtime;
- normal Velopack application validation with no `--skipVeloAppCheck`;
- installed `--update-check` and `--update-now` engineering commands;
- hardened telemetry startup/capture markers.

The alpha.3 validation succeeded: an installed public alpha.2 detected, downloaded and applied alpha.3 through the application-driven update path, after which alpha.3 launched normally and reported no newer update.
