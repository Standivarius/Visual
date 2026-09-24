# Dify Cloud setup for Visual Support Alpha

This document describes the reproducible cloud-only support prototype currently deployed for Visual.

## Cloud resources

Create or maintain:

- Dify Cloud app: `Visual Support Alpha`;
- knowledge base: `Visual Support Knowledge Economical`;
- model provider: verified Dify `OpenAI-API-compatible` plugin;
- test model: `muse-spark-1.3-contributor`;
- API base: `https://api.meta.ai/v1`.

Store the Meta provider credential in Dify Cloud model-provider settings only. Never store it in this repository, `Visual.exe`, support bundles, or public CI variables.

## Build the knowledge upload bundle

The reviewed source documents live under `support/knowledge/visual/`.

Generate the one-file Dify upload bundle with:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\support\dify\build-knowledge.ps1
```

Upload `support/dify/visual-support-knowledge.md` to the knowledge base.

Use **Economical** indexing. This creates an inverted/full-text index and avoids a dependency on Dify-hosted embedding credits.

## Retrieval node

Configure the Knowledge Retrieval node with:

- dataset: `Visual Support Knowledge Economical` only;
- retrieval mode: multiple retrieval;
- Top K: `1`;
- reranking: disabled;
- metadata filtering: disabled unless a reviewed support design later requires it.

Do not attach the older High Quality dataset to the same retrieval node; doing so can reintroduce embedding/reranking provider dependencies.

## LLM node

Use `support/dify/system-prompt.md` as the reviewed instruction source.

In Dify, enable LLM context and bind it to `Knowledge Retrieval.result`. Ensure the retrieved-context variable is present in the prompt. The deployed workflow uses the Knowledge Retrieval output as evidence before general reasoning.

Model:

- provider: `OpenAI-API-compatible`;
- model: `Muse Spark 1.3 Contributor` / `muse-spark-1.3-contributor`;
- mode: chat.

## User-facing features

Published test configuration:

- citations/retriever resources: enabled;
- Visual-specific opening statement and suggested questions: enabled;
- file upload: disabled;
- speech features: disabled.

File upload stays disabled until the workflow contains a tested diagnostics extraction/privacy path.

## Acceptance test

Run every case in `support/dify/evaluation-cases.md` against the final draft using Dify's draft-run API or Preview.

Do not publish a changed workflow until all six cases pass again.

The 2026-09-24 Contributor run is recorded in `evaluation-results-2026-09-24.md`.

## Publication

The validated workflow was published on 2026-09-24 with the label `Support alpha final`.

Treat this as a test/support prototype, not a final customer support channel. Before accepting sensitive diagnostics or broader external traffic, review model-data terms, retention, file ingestion, authentication and the server-side integration boundary.
