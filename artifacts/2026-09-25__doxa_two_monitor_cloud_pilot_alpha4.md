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

## Dify completion

A fresh Dify Cloud Chatflow app named `Doxa Installer Planner` was created in the currently authenticated Dify workspace. App ID: `4568517a-3cec-4f05-a4f4-add9f177206a`.

The verified `OpenAI-API-compatible` provider was installed and configured with `Muse Spark 1.3 Contributor` / `muse-spark-1.3-contributor`. The Meta provider credential remains server-side in Dify/model-provider configuration and is not present in the client package.

Dify Preview returned the expected Muse test response. The app was published, a Dify Service API key was created, and that key was stored only in the MARIUS-DELL user environment for the lab backend. A direct authenticated `POST /chat-messages` request to `https://api.dify.ai/v1` returned the expected response, proving the published Dify -> Muse path.

The final public planner chain was then proven as:

`two-monitor pilot -> temporary Doxa HTTPS proxy -> Dify -> Muse -> allowlisted planner action -> local Visual follow-up health check`.

A clean reset removed the prior Visual/Doxa test state. The final portable alpha.4 kit then reinstalled Visual from scratch, passed the real two-monitor WGC/D3D health check, used the Dify-backed public planner, received an allowlisted action, executed the local follow-up, and passed. Pilot exit code: `0`.

## Remaining boundary

The temporary HTTPS tunnel and expendable client token are lab-only and intentionally not committed. Production still requires a stable Doxa-owned HTTPS endpoint and production credential management. Physical Silicon Motion/Doxa hardware integration remains deferred until the unit is available.
