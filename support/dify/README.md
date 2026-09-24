# Dify Cloud support prototype

## Purpose

`Visual Support Alpha` is the hosted cloud support prototype for Visual alpha testing. Dify Cloud provides the chatflow, retrieval layer and model-provider integration. No local LLM, local Dify server or customer-side model key is required.

The repository remains authoritative for reviewed support knowledge under `support/knowledge/`.

## Deployed test configuration

As of 2026-09-24:

- Dify Cloud app: `Visual Support Alpha`;
- app type: Chatflow / advanced chat;
- test LLM: `muse-spark-1.3-contributor`;
- provider integration: Dify verified `OpenAI-API-compatible` plugin;
- Meta API base: `https://api.meta.ai/v1`;
- model/provider key: stored in Dify Cloud only, never in this repository or `Visual.exe`;
- knowledge base: `Visual Support Knowledge Economical`;
- indexing: Economical / inverted index;
- retrieval: one knowledge base, multiple-retrieval mode with reranking disabled and Top K = 1;
- citations/retriever resources: enabled;
- published workflow label: `Support alpha final`.

The Economical knowledge base is intentional. The original High Quality knowledge base depended on Dify-hosted embedding/reranking credits. The current test path avoids that dependency and uses full-text retrieval instead.

## Knowledge upload

Dify Sandbox allowed only one document in the initial upload flow, so `visual-support-knowledge.md` is a generated deployment bundle made from the reviewed Markdown files under `support/knowledge/visual/`.

Regenerate it with:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\support\dify\build-knowledge.ps1
```

Review/update the source files first; do not treat the generated bundle as the authoritative source.

## Safety boundary

The prototype may:

- retrieve reviewed Visual support knowledge;
- interpret diagnostics explicitly pasted into the conversation;
- explain likely causes and known limitations;
- recommend documented reversible checks;
- escalate when evidence is insufficient.

It must not:

- run arbitrary commands on tester PCs;
- hold administrator credentials;
- disable security or invent driver/firmware changes;
- request passwords, API keys or arbitrary personal files;
- claim undocumented repairs as approved.

## Diagnostic files

Dify file upload is intentionally **disabled** in the published test workflow. The current chatflow does not yet contain a reviewed document-extraction/privacy path, so enabling an upload button would imply support that is not actually wired.

For now, use the structured, non-sensitive fields from `diagnostics.json` by pasting the relevant excerpt into the support conversation. A later workflow can add file ingestion after extraction, privacy and retention behavior are tested end to end.

## Evaluation

All six cases in `evaluation-cases.md` passed against `muse-spark-1.3-contributor` on 2026-09-24. See `evaluation-results-2026-09-24.md`.

## Later Visual integration

A future in-app `Ask Support` action should call a small authenticated server-side proxy which then calls Dify. Keep Dify and model-provider credentials out of the public Windows executable.
