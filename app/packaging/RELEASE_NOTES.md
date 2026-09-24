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
