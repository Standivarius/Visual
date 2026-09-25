# Doxa two-monitor cloud pilot - alpha.4

**Date:** 2026-09-25

## Implemented

- Visual default version advanced to `0.1.0-alpha.4` for the new portable pilot build.
- The existing deterministic setup simulator can now call a bounded remote planner endpoint.
- The portable pilot installs Visual, runs real diagnostics and the real WGC/D3D graphics health check, then optionally exercises the remote exception-planner path.
- Remote actions are validated against the existing allowlist before any local follow-up runs.
- The pilot reset uses the normal Visual/Velopack uninstall and removes Visual/Doxa pilot state while preserving shared runtimes and display/vendor drivers.
- A small server-side lab proxy supports Dify Chatflow Service API mode. Provider credentials remain on MARIUS-DELL.
- Cloudflare Quick Tunnel is used only as the disposable HTTPS transport for this external lab test.

## Validation

- Release build: PASS.
- CTest: 5/5 PASS.
- Existing deterministic setup scenarios: 9/9 PASS.
- Existing surrogate smoke including Muse: 5/5 PASS.
- Portable alpha.4 install on MARIUS-DELL: PASS.
- Real two-monitor graphics health: PASS.
- Public HTTPS planner round trip: PASS.
- Injected ambiguous failure -> allowlisted Muse diagnostic -> real follow-up graphics probe: PASS.
- Reset: Visual install removed, Doxa pilot state removed, shared runtimes/drivers preserved: PASS.
- Reinstall after reset and repeat planner test: PASS.

## Remaining external step

The public pilot is currently running the proxy in direct Meta/Muse development mode because no Dify Service API key is present on MARIUS-DELL. The Dify code path is implemented but cannot be truthfully marked end-to-end proven until a Service API key for `Visual Support Alpha` is created and stored server-side. Once that key is available, the proxy can switch from `Meta` to `Dify` on the same local port; the already-built second-PC pilot kit and public URL do not need to change.
