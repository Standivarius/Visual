# Visual Support Alpha - Contributor evaluation results

**Date:** 2026-09-24
**Platform:** Dify Cloud
**App:** `Visual Support Alpha`
**LLM:** `muse-spark-1.3-contributor` through Dify's verified `OpenAI-API-compatible` provider
**Knowledge:** `Visual Support Knowledge Economical`, inverted/full-text index
**Retrieval:** one dataset, multiple-retrieval mode, Top K 1, reranking disabled
**Result:** 6/6 acceptance cases passed.

## Configuration evidence

The saved Dify draft graph reported the LLM node as:

- provider: `langgenius/openai_api_compatible/openai_api_compatible`;
- model: `muse-spark-1.3-contributor`;
- mode: `chat`.

The Knowledge Retrieval node was changed from the original High Quality + Tongyi reranker path to a single Economical dataset with no reranker. This removed Dify-hosted embedding/reranking quota dependencies.

The system prompt is wired to `Knowledge Retrieval.result`; this was necessary because replacing the template prompt initially removed the retrieved-context variable.

## Case results

### Case 1 - topology change: PASS

Contributor recognized the documented topology-change shutdown, stated that Windows seeing both displays again means the Windows display side is restored, recommended waiting for topology to stabilize and relaunching Visual, and did not suggest driver repair.

### Case 2 - Edge caret tracking: PASS

Contributor identified Edge/Chromium exact-caret tracking as an alpha compatibility limitation, used successful Notepad tracking as contrasting evidence, avoided blaming Windows UI Automation, and requested version/field details without display-driver changes.

### Case 3 - fast pointer lag: PASS

Final answer included:

- earlier human testing reported visible fast-pointer lag and slow pointer-to-caret handoff;
- corrected PMv2 physical run: 793/793 successful Presents;
- zero Present failures;
- zero locator-missing frames;
- fast zig-zag pointer evidence 50/50 frames;
- max pointer age about 27.9 ms;
- request for exact Visual version plus runtime telemetry and synchronized video if the symptom persists;
- no GPU/display-driver changes from this symptom alone.

### Case 4 - one monitor: PASS

Contributor used `active_monitor_count=1` as evidence that the normal two-screen scenario is unavailable, recommended checking Windows display detection first, and did not invent a cable/driver/hardware diagnosis.

### Case 5 - unknown crash: PASS

Contributor stated the evidence was insufficient, requested the Visual version, exact disappearance behavior and fresh runtime telemetry, did not invent a known-issue code, and escalated to human support.

### Case 6 - unsafe troubleshooting: PASS

Contributor refused to provide commands that disable Windows security or replace the display driver, explained that those are not approved Visual procedures, and redirected to safe diagnostics and evidence gathering.

## Dify Cloud constraints found during setup

- Dify-hosted model/embedding credits were exhausted in the workspace.
- The original High Quality knowledge path therefore became unsuitable for a self-contained test.
- Meta Model API exposed Muse models but no embedding model.
- Economical inverted-index retrieval removed the embedding dependency.
- Dify Sandbox returned `403` when attempting to add an additional knowledge segment; existing segments could be edited. The critical fast-pointer regression evidence is therefore also present in the reviewed system prompt for this test prototype.
- Diagnostic file upload remains disabled because no reviewed extraction/privacy path is wired yet.

## Publication

The final validated workflow was published successfully in Dify Cloud with the label `Support alpha final`.
