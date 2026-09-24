# Visual public alpha.2 -> alpha.3 update proof

Date: 2026-09-24
Machine: `MARIUS-DELL`
Repository: `C:\dev\Visual`

## Public releases

- source client: `v0.1.0-alpha.2`
- update target: `v0.1.0-alpha.3`
- alpha.3 release workflow: completed successfully
- alpha.3 public release: GitHub prerelease published successfully

## Installed alpha.2 check

The installed public alpha.2 executable reported:

- `installed_version=0.1.0-alpha.2`
- `available_version=0.1.0-alpha.3`
- `result=update_available`
- process exit code `10`

## Installed alpha.2 apply

Running installed `visual_app.exe --update-now` reported:

- `update_now_start`
- `installed_version=0.1.0-alpha.2`
- `available_version=0.1.0-alpha.3`
- `download_start`
- `download_complete`
- `result=apply_scheduled`

The installed tree subsequently changed to ProductVersion `0.1.0-alpha.3`.

The Velopack package cache contains `Standivarius.Visual-0.1.0-alpha.3-alpha-full.nupkg` with alpha.3 package metadata. Its reconstructed ZIP bytes differ from the public full-package ZIP, which is expected when a delta is applied, but the installed executable itself matches the public release payload exactly.

Public alpha.3 full-package SHA-256:

`32046ba54a4a24dc03d32cd0522039869dad0da760b8a89d71655d0c204d2164`

`visual_app.exe` SHA-256 from the public alpha.3 full package:

`5e3c4d0e137738c6fc7faa839f66f6537823abb2e284c131db5e76e1c6e0e29b`

Installed alpha.3 `visual_app.exe` SHA-256:

`5e3c4d0e137738c6fc7faa839f66f6537823abb2e284c131db5e76e1c6e0e29b`

The hashes match exactly.

## Updated alpha.3 validation

The updated installed alpha.3 passed:

- `visual_app.exe --update-check` exit `0`;
- `installed_version=0.1.0-alpha.3`;
- `result=no_update`;
- ordinary launch remained alive after four seconds;
- telemetry emitted `telemetry_started`, `capture_started`, `first_source_frame`, and `first_render_frame`;
- global `Ctrl+Alt+Q` caused a graceful process exit code `0`.

Fresh launch telemetry is preserved locally at:

`artifacts/alpha3_normal_launch_telemetry.csv`

## Conclusion

The native Velopack application-driven update transport is proven end to end for public `alpha.2 -> alpha.3`: check, detect, download, schedule apply, replace the installed version, run the updated public binary, and confirm it is current against the public feed.

What remains is product UX/policy around update prompts, timing, restart behavior, settings and background checks - not the update transport itself.
