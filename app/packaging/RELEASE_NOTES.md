# Unreleased

Visual now has the first user-facing low-vision workspace layer on top of the proven capture/tracking engine. It adds 1.5x and 3x zoom, an accessible native settings window, persisted Context/Detail/Reference display roles, independent pointer/caret/focus follow and marker controls, a capture-excluded Context-screen Detail View rectangle, and Normal/High contrast/Inverted/Grayscale Detail appearance modes. Monitor-role changes apply on the next Visual start; the other settings apply live.

The installer/update engine remains separate from these Visual workspace features.

# Visual 0.1.0-alpha.6

Visual 0.1.0-alpha.6 fixes pointer-versus-caret/focus arbitration discovered during the ASUS two-monitor test.

Stationary caret and focus observations no longer become fresh user intent merely because UI Automation or Win32 sampled them again. Their activity timestamp is retained until the target actually moves or changes. In addition, each real pointer movement establishes an intent barrier: caret/focus evidence from before that movement cannot reclaim the viewport after pointer freshness expires; only a genuinely newer caret/focus change can take control again. This prevents a blinking/stationary text caret from pulling the magnified viewport back after deliberate mouse movement, while real keyboard caret movement and focus changes still resume tracking normally.

Validation on MARIUS-DELL: all five CTests pass; Chromium editing smoke passes with 150 semantic-caret frames and zero present failures; fast-pointer stress passes; and a targeted stationary-caret test observed zero caret/focus reclaim frames during the 150-650 ms interval after pointer movement stopped.

# Visual 0.1.0-alpha.5

Visual 0.1.0-alpha.5 is the first normal-install Doxa cloud-assistance pilot.

A standard Velopack installation now performs the Doxa setup check automatically on the first installed launch. It verifies the two-display Windows baseline, runs the real bounded WGC/D3D graphics health check, sends only structured setup state to the permanent Doxa Cloudflare Worker, and follows only approved diagnostic/escalation actions. The Worker keeps the Dify Service API key server-side and calls the published `Doxa Installer Planner` app, which uses Muse Spark 1.3 Contributor for ambiguous failures.

Healthy installations complete deterministically after the cloud connection is verified; Dify/Muse is invoked only when the deterministic checks leave an ambiguous problem. Setup state is retained under `%LOCALAPPDATA%\Standivarius\Visual\doxa-setup`, outside the replaceable Velopack install tree, so failed first runs can be retried and successful setup state survives updates.

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
