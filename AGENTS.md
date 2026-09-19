# Visual Agent Guide

## Canonical environment

- Machine: `MARIUS-DELL`
- Canonical repository: `C:\dev\Visual`
- Repository/project identity: `Visual`
- Product: **Visual**, a Windows low-vision magnification application associated with Doxa

Treat this repository as the authoritative project state. Inspect relevant files before making claims about the current implementation.

## Purpose

Visual is a Windows low-vision magnification project. The goal is a reliable, high-performance Windows magnifier with excellent tracking, viewport behaviour, coordinate correctness, multi-monitor support, and measurable accessibility behaviour.

Do not optimize for superficial feature-count parity with incumbent products. Optimize for observable usefulness, compatibility, latency, predictability, and evidence-backed engineering decisions.

## Clean-room rule

This project may study:

- public Windows documentation and APIs;
- publicly documented competitor behaviour;
- behaviour visible through legitimate ordinary use;
- legitimately licensed open-source implementations.

Do not obtain, reproduce, decompile, or incorporate proprietary source code, proprietary assets, confidential implementation details, or other non-public competitor material.

Keep a clear distinction between:

- observed behaviour;
- public documentation;
- licensed open-source material and its provenance;
- Visual's independent design and implementation.

For any third-party source considered for reuse, record the repository/source, exact commit or tag, retrieval date, licence, relevant licence-file hash where useful, files reused, modifications, and dependency licensing.

## Evidence-first workflow

Use this sequence for material technical decisions:

`research -> hypothesis -> experiment -> evidence -> architectural decision -> production implementation`

Do not turn a plausible hypothesis into production architecture merely by naming classes, modules, or folders around it.

When uncertainty can be resolved by repository inspection or a safe test, resolve it rather than asking the user to investigate manually.

## Current technical direction

Unless experiments provide contrary evidence:

- Windows-native implementation;
- modern C++;
- Win32 and C++/WinRT;
- Direct3D 11 as the initial rendering baseline;
- CMake;
- MSVC / Visual Studio and the Windows SDK;
- Python and PowerShell for research/test support where useful.

The capture backend is deliberately **undecided**. Carry both of these through comparable measurement spikes:

- Windows.Graphics.Capture + D3D11;
- DXGI Desktop Duplication + D3D11.

Treat the Windows Magnification API primarily as a behavioural/reference implementation and possible fallback candidate until evidence says otherwise.

Do not introduce D3D12 unless profiling or an experiment demonstrates a concrete need.

Keep two major pipelines conceptually separate:

1. graphics: capture -> GPU texture -> scaling/image processing -> presentation;
2. intent/tracking: mouse/caret/focus/window signals -> Point-of-Interest engine -> viewport controller -> graphics pipeline.

Do not bury Point-of-Interest policy inside capture/render code.

## Early engineering priorities

Prioritize measurable behaviour in this order:

- reliable low-latency screen capture and GPU presentation;
- mouse tracking;
- keyboard-focus tracking;
- text-caret tracking and compatibility;
- Point-of-Interest arbitration;
- viewport motion, dead zones, hysteresis, margins, and conflict handling;
- coordinate/input correctness under magnification;
- mixed-DPI and multi-monitor behaviour;
- visual enhancements relevant to low-vision users;
- deterministic behavioural testing.

Advanced semantic text rerendering, speech, OCR runtime, broad app-specific hacks, polished settings UI, installer work, and large feature matrices are not prerequisites for proving the initial architecture.

## Implementation rules

Before modifying existing code:

1. inspect the relevant implementation and nearby dependencies;
2. understand project conventions and the specific question being solved;
3. preserve unrelated existing work;
4. make the smallest coherent correction or addition;
5. build or test it where practical;
6. report what changed, what the evidence shows, and what remains uncertain.

Prefer simple Windows-native mechanisms over unnecessary frameworks or abstraction layers.

Do not add abstractions merely because they might be useful later.

## Spike and experiment rules

Prototype/spike work belongs outside the production application until it has earned its way into `app/`.

For every spike:

1. state the question it answers;
2. define minimum scope;
3. define measurements and acceptance criteria before implementation;
4. build only enough to answer the question;
5. run the experiment;
6. store raw results separately from interpretation;
7. interpret the evidence;
8. recommend graduation to production only after the evidence supports it.

Comparable alternatives must use comparable workloads, instrumentation, and metrics.

## Testing philosophy

Prefer deterministic Windows tooling for behavioural tests. A model may decide what to investigate next, but measurement and execution should be reproducible without dependence on one model provider.

High-value tests include capture/frame pacing, overlay recursion, drag/click coordinate correctness, mouse edge tracking, Tab/focus navigation, long-line caret tracking, mouse-versus-caret conflict, mixed DPI, monitor hot-plug, and display/fullscreen transitions.

Measure geometry, timing, frame age, movement thresholds, final margins, overshoot, and recovery behaviour rather than relying on impressions such as "seems smooth".

## Repository and Git safety

- The canonical workspace is only `C:\dev\Visual`.
- Do not access or modify unrelated repositories or user files through project tooling.
- Preserve unrelated local work.
- Do not run destructive Git operations such as reset, clean, forced checkout, history rewriting, or mass deletion unless the user explicitly authorizes them.
- Do not commit, push, or mutate remote state unless explicitly requested.

## Reporting vocabulary

When reporting findings, distinguish explicitly between:

- **Observed repository fact** — directly present in the current workspace;
- **Test result** — reproduced by a named test or command;
- **Documented external fact** — supported by a public source;
- **Hypothesis** — plausible but not yet demonstrated;
- **Recommendation** — proposed next action based on the available evidence.

Do not claim work is complete unless repository/tool evidence confirms it.

## Artifact naming convention

All new project artifacts under `artifacts/` must use:

`YYYY-MM-DD__agent-name__relevant_content_vMAJOR.MINOR.ext`

Use lowercase agent identifiers and concise underscore-separated content descriptors. Example:

`2026-09-18__gpt-5.6-sol__agentic_audit_m2_cutover_map_v0.1.md`

Increment the version when materially revising the same artifact rather than creating ambiguous names such as `final`, `new`, or `latest`.
