# Dify Cloud setup for Visual Support Alpha

The first Visual support prototype should use Dify Cloud. No VPS is required.

## One-time account setup

1. Sign in to Dify Cloud with the account that will own the Visual support prototype.
2. Create a knowledge base named `Visual Alpha Support`.
3. Upload the Markdown files under `support/knowledge/`.
4. Create a Chatbot/Chatflow named `Visual Support Alpha`.
5. Attach the `Visual Alpha Support` knowledge base as its retrieval source.
6. Use `support/dify/system-prompt.md` as the assistant system instruction.
7. Enable file/document upload if the selected Dify app mode supports it.
8. Use an available strong model from the Dify workspace for the first proof-of-concept. Do not add a separate provider API key unless Dify credits/model availability require it.

## If a model API key is needed

Configure the provider key directly in Dify's model-provider settings.

Do not:

- paste the provider key into ChatGPT;
- store it in this repository;
- put it in `Visual.exe`;
- put it in a public GitHub Actions variable.

The first prototype should be used through the hosted Dify UI, so Visual itself needs no Dify key.

## Load the knowledge base

Initial source documents are under:

- `support/knowledge/README.md`
- `support/knowledge/visual/product-overview.md`
- `support/knowledge/visual/installation.md`
- `support/knowledge/visual/diagnostics.md`
- `support/knowledge/visual/known-issues.md`
- `support/knowledge/visual/troubleshooting/`

Keep the repository copies authoritative. If a support rule changes, update/review it here first and then refresh the Dify knowledge base.

## First diagnostic workflow

For the first prototype:

1. tester runs Visual diagnostics/support bundle;
2. support operator receives `diagnostics.json` or the support ZIP;
3. if necessary, extract `diagnostics.json` locally;
4. attach the JSON to the Dify conversation;
5. describe the user's symptom;
6. the assistant combines the diagnostic evidence with retrieved Visual support knowledge;
7. if evidence is insufficient, it must escalate instead of inventing a fix.

## Acceptance test

Run every case in `support/dify/evaluation-cases.md`.

Do not invite outside testers until the assistant consistently:

- recognizes documented topology behavior;
- handles Edge caret limitations conservatively;
- does not claim the fast-pointer issue is universally fixed;
- treats single-monitor diagnostics as an environment/topology fact rather than proof of hardware failure;
- escalates unknown crashes;
- refuses unsafe driver/security commands.

## Later integration

Only after the hosted prototype is useful should Visual gain an integrated `Ask Support` action.

At that point use a small server-side/backend proxy that holds the Dify API credential. Never embed the Dify API key or model-provider key in the public Windows executable.
