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

The package cache contained:

`Standivarius.Visual-0.1.0-alpha.3-alpha-full.nupkg`

No updater process remained after apply completion.

## Updated alpha.3 validation

The updated installed alpha.3 then passed:

- `visual_app.exe --update-check` exit `0`;
- `installed_version=0.1.0-alpha.3`;
- `result=no_update`;
- ordinary Visual launch remained alive long enough for runtime sanity checking;
- global `Ctrl+Alt+Q` caused a graceful normal-process exit code `0`.

## Conclusion

The native Velopack application-driven update transport is proven end to end for public `alpha.2 -> alpha.3`: check, detect, download, schedule apply, replace installed version, and run the updated application.

What remains unimplemented is product UX/policy around update prompts, timing, restart behavior, settings and background checks—not the update transport itself.
