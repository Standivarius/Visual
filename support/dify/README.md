# Dify support prototype

## Purpose

Use Dify Cloud initially as the hosted RAG/support layer for Visual alpha testers. No VPS is required for this first prototype.

The repository remains the authoritative source for support knowledge under `support/knowledge/`.

## Initial Dify setup

Create one Dify Knowledge Base named:

`Visual Alpha Support`

Upload the Markdown files under:

`support/knowledge/`

Create one Chatbot/Chatflow named:

`Visual Support Alpha`

Attach the `Visual Alpha Support` knowledge base and use `system-prompt.md` as the assistant instructions.

Enable file upload so a tester/support operator can attach the deterministic `visual-support-*.zip` or its `diagnostics.json` file. In the first prototype, the operator may unzip the bundle and upload `diagnostics.json` directly if Dify does not automatically extract ZIP contents in the selected workflow.

## Model choice

For the first proof-of-concept, use any strong general model available in the Dify workspace. The architecture intentionally does not depend on one provider.

If Dify Cloud credits are insufficient, configure the chosen provider key directly in Dify's model-provider settings. Do not store provider keys in this repository or inside Visual.

## Security boundary

The first Dify prototype is advisory only.

It may:

- retrieve Visual support knowledge;
- interpret structured diagnostics supplied by the user;
- explain likely causes;
- recommend documented reversible checks;
- escalate to human support.

It must not:

- run arbitrary shell/PowerShell commands on a tester PC;
- hold administrator credentials;
- install drivers/firmware;
- receive GitHub/provider API keys from users;
- claim undocumented repairs as approved.

## Later automation

Once the manual prototype is useful, Dify's Knowledge API can synchronize repository documents programmatically. API keys must remain server-side. Do not embed a Dify API key in the public Windows client.

A future in-app `Ask Support` feature should call a small authenticated backend/serverless proxy, which then calls Dify. That keeps Dify/model secrets out of `Visual.exe`.

## Prototype validation cases

Before using the assistant with outside testers, test it against at least these cases:

1. HDMI/topology-change shutdown — should advise restore topology and relaunch, not reinstall drivers.
2. Edge caret fallback — should identify this as a known alpha compatibility limitation.
3. Fast pointer/caret lag report — should ask for version/telemetry and not assume the old defect persists.
4. Single-monitor diagnostics — should explain that the normal two-screen scenario is unavailable without claiming hardware failure.
5. Unknown issue — should explicitly say evidence is insufficient and escalate rather than inventing a fix.
