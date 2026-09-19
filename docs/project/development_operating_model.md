# Visual Development Operating Model

## Ownership

- **GPT-5.6 Sol is the primary developer and integration owner.**
- The primary developer inspects the canonical repository, makes or reviews implementation changes, runs/coordinates builds and tests, and reports evidence.
- Architecture ownership stays with the primary developer unless the user explicitly changes that arrangement.

## Canonical workspace

- Machine: `MARIUS-DELL`
- Canonical repository: `C:\dev\Visual`
- The canonical repository is authoritative.
- Do not modify unrelated repositories or user files.

## Delegation

Delegation is for **specific bounded tasks**, not general project ownership.

Good delegation examples:

- independent code review;
- adversarial review of an implementation or test result;
- investigation of a narrow Windows API question;
- implementation of a small isolated component with explicit file scope;
- writing or extending narrowly scoped tests;
- reproducing a named bug in an isolated area;
- comparing two approaches against a fixed rubric.

Avoid delegating:

- vague instructions such as “build Visual”;
- overlapping writes to the same files;
- final architecture selection without primary-developer review;
- unbounded refactors;
- Git history or remote operations.

### Gemini / Antigravity

- Direct Gemini delegation is operational through the Google Antigravity CLI.
- Verified CLI: `C:\Users\DELL\AppData\Local\agy\bin\agy.exe`, version `1.2.7`.
- Verified authenticated headless execution on 2026-09-19.
- Verified Gemini models include `gemini-3.8-flash-high`, `gemini-3.8-flash-medium`, `gemini-3.8-flash-low`, `gemini-3.1-pro-high`, and `gemini-3.1-pro-low`.
- Default delegated Gemini model: `gemini-3.8-flash-high`.
- Read-only Gemini tasks run inside an isolated disposable snapshot of `C:\dev\Visual`; `.env`, `.git`, capture artifacts and `node_modules` are excluded from the snapshot.
- The bridge captures substantive stdout/stderr, task status, exit code and supports cancellation.
- A smoke test successfully read `AGENTS.md` from the isolated snapshot and returned the expected repository identity with exit code `0`; canonical repo task delta was empty.
- Writable Antigravity delegation remains intentionally disabled until explicit path-scoped write enforcement is implemented.

### Muse

- Muse is part of the intended delegation pool, but the current repository bridge does not expose a Muse provider or callable model.
- Do not claim Muse work has been delegated until a verified invocation and result-handoff path exists.
- Establishing Muse delegation is a one-time tooling prerequisite, not a blocker for ordinary Visual development.

## Concurrency and write safety

- **One writer to the canonical workspace at a time.**
- Delegated agents are read-only by default.
- If a delegated agent receives write authority, it must be limited to explicit repository-relative paths.
- Do not allow two agents to modify overlapping paths concurrently.
- For genuinely concurrent implementation, use isolated worktrees or an equivalent explicitly separated workspace scheme before granting write authority.
- The primary developer reviews all delegated changes before they are accepted as project state.

## Verification rule

Never accept an agent's statement that work succeeded as evidence by itself.

For delegated or primary work, verify as applicable with:

1. repository diff/change inspection;
2. compilation/build result;
3. automated tests;
4. experiment output or raw measurements;
5. file existence/content checks.

Report delegated conclusions as hypotheses until independently verified where practical.

## Git rule

- No destructive Git operations.
- No commit, push, branch rewrite, reset, clean, or remote mutation unless explicitly authorized.
- Before concurrent or delegated write work becomes routine, establish a clean baseline commit so changes can be attributed and reviewed reliably.

## Spike workflow

Every technical spike must have a written charter before code is added:

1. hypothesis/question;
2. minimum implementation scope;
3. environment under test;
4. measurements;
5. success/falsification criteria;
6. allowed files/directories;
7. build/test command;
8. raw-result location;
9. interpretation location.

The spike must answer the question before it grows into production code.

## Experiment environment record

Before Spike 1, record at least:

- Windows version/build;
- CPU;
- GPU(s) and driver version;
- monitor count/model if available;
- resolution, refresh rate, DPI/scaling, orientation and topology;
- Visual Studio/MSVC version;
- Windows SDK version;
- CMake version;
- whether Ninja is available;
- Git and PowerShell versions;
- whether WGC and D3D11 development headers/libraries are present;
- whether HDR is enabled;
- whether monitors are native GPU outputs or USB/display-adapter outputs.

This allows performance and compatibility results to be reproduced later.

## First-spike scope discipline

Spike 1 should not include:

- settings UI;
- installer work;
- Doxa-specific code;
- caret/focus tracking;
- application profiles;
- workspace restoration;
- HDR support beyond recording the environment state;
- production abstractions not required by the experiment.

The first capture/render spike should prove or falsify only the core cross-monitor technical premise on ordinary Windows dual-monitor hardware.
