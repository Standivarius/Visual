# Doxa lab cloud planner

`lab-proxy.ps1` is a temporary test backend for the Doxa installer pilot. It is not the production cloud service.

The second PC authenticates to this proxy with an expendable lab token. The proxy holds the model-provider credential and calls either Dify Cloud or Meta/Muse. Provider credentials never belong in the pilot ZIP.

## Current lab app

The installer pilot now uses the published Dify Chatflow `Doxa Installer Planner` (app ID `4568517a-3cec-4f05-a4f4-add9f177206a`) with the `Muse Spark 1.3 Contributor` model through the verified OpenAI-compatible provider.

The app API key is stored server-side on MARIUS-DELL for the lab and must never be copied into the pilot ZIP.
## Dify mode

Create an app API key inside the Dify app. To avoid putting the key in chat or command history, store it locally on MARIUS-DELL with:

```powershell
.\support\doxa-cloud\configure-dify-key.ps1
```

The helper prompts locally, does not print the key, and stores it only in the current Windows user's environment for this lab. Use `-Clear` after the pilot if desired. Then run:

```powershell
$env:DOXA_LAB_TOKEN = '<temporary-random-token>'
.\support\doxa-cloud\lab-proxy.ps1 -Port 8791 -Provider Dify
```

The proxy uses Dify Cloud's app API base `https://api.dify.ai/v1` and `POST /chat-messages` in blocking mode. It sends the structured installer state inside a constrained planner prompt and accepts only actions present in `app/packaging/doxa-setup/approved-actions.json`.

### Planner audit evidence

The permanent Worker returns the Dify `task_id` (when present), `message_id`/`id`, and `conversation_id` with every `ai_plan`. These are execution identifiers returned by Dify and are safe to retain in setup evidence.

The Worker also returns the configured, non-secret planner identity from tracked Worker configuration: app ID/name and model ID/name (`muse-spark-1.3-contributor` / `Muse Spark 1.3 Contributor`). These fields are explicitly labelled as configured identity (`model_identity_source=worker_config`); they are not presented as model-name fields returned by the Dify Service API. The Dify message/conversation IDs are the transaction trace keys to correlate with Dify/observability logs when audit-grade proof of the executed model is required.

The Windows setup orchestrator stores the full Worker response in `cloud-response.json` and logs `cloud_ai_trace` with the configured model ID plus Dify message/task/conversation IDs. No provider credential is written to client logs.

For temporary cross-machine testing, expose only the proxy port with an HTTPS tunnel such as Cloudflare Quick Tunnel. Do not expose the repository bridge as the installer endpoint.

## Meta fallback

`-Provider Meta` exists only to validate the planner transport when a Dify app API key is unavailable. It uses `META_API_KEY` on the proxy host. The production direction remains client -> Doxa-owned HTTPS service -> Dify/Muse.

## Security boundary

The client sends a small structured state object, not arbitrary command output. The model chooses from an allowlist of diagnostic/escalation actions. The client validates the action again before executing it. Version 1 gives the model no arbitrary command execution, registry editing, security-policy bypass, driver replacement, or unrestricted download capability.
