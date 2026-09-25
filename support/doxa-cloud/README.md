# Doxa lab cloud planner

`lab-proxy.ps1` is a temporary test backend for the Doxa installer pilot. It is not the production cloud service.

The second PC authenticates to this proxy with an expendable lab token. The proxy holds the model-provider credential and calls either Dify Cloud or Meta/Muse. Provider credentials never belong in the pilot ZIP.

## Dify mode

Set `DIFY_API_KEY` on the proxy host to an app API key created inside the Dify app, then run:

```powershell
$env:DOXA_LAB_TOKEN = '<temporary-random-token>'
.\support\doxa-cloud\lab-proxy.ps1 -Port 8791 -Provider Dify
```

The proxy uses Dify Cloud's app API base `https://api.dify.ai/v1` and `POST /chat-messages` in blocking mode. It sends the structured installer state inside a constrained planner prompt and accepts only actions present in `app/packaging/doxa-setup/approved-actions.json`.

For temporary cross-machine testing, expose only the proxy port with an HTTPS tunnel such as Cloudflare Quick Tunnel. Do not expose the repository bridge as the installer endpoint.

## Meta fallback

`-Provider Meta` exists only to validate the planner transport when a Dify app API key is unavailable. It uses `META_API_KEY` on the proxy host. The production direction remains client -> Doxa-owned HTTPS service -> Dify/Muse.

## Security boundary

The client sends a small structured state object, not arbitrary command output. The model chooses from an allowlist of diagnostic/escalation actions. The client validates the action again before executing it. Version 1 gives the model no arbitrary command execution, registry editing, security-policy bypass, driver replacement, or unrestricted download capability.
