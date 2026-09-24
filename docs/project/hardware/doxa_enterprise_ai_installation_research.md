# Doxa enterprise AI-assisted installation research

**Date:** 2026-09-25
**Scope:** enterprise deployment, endpoint remediation, OEM support, AI-assisted endpoint operations, and open-source reuse relevant to Doxa.

## Executive conclusion

There is no single widely adopted category called an "AI installer" that Doxa should copy. The mature pattern is a combination of:

1. standard enterprise software deployment under IT control;
2. deterministic endpoint detection and health checks;
3. approved remediation actions;
4. AI used only to diagnose ambiguous endpoint state and select among approved actions;
5. explicit escalation when policy or permissions block progress.

This is already visible in modern endpoint-management products. Nexthink Spark describes real-time endpoint telemetry, AI diagnostics and **IT-approved remediation**. Tanium describes governed AI-assisted actions over real-time endpoint data. ControlUp and 1E similarly close the loop from observation to approved automation/self-healing. Dell SupportAssist for Business PCs combines telemetry, proactive detection, Dell-authored remediation scripts and enterprise deployment/management. These systems do not treat the model as an unrestricted local administrator.

For Doxa, the architecture can be simpler because Doxa should control most hardware, driver and application variance.

## What established systems actually do

### Microsoft Intune Win32 apps and Remediations

Microsoft Intune already solves the enterprise mechanics Doxa should not recreate:

- deployment in user or SYSTEM context;
- requirement rules;
- detection rules;
- dependencies;
- supersedence;
- return-code handling and retries;
- silent deployment without a logged-on user;
- detect/remediate script packages for common support issues.

Remediations explicitly split detection from remediation, and remediation runs only when detection reports a problem.

Sources:

- https://learn.microsoft.com/en-us/mem/intune/apps/apps-win32-add
- https://learn.microsoft.com/en-gb/intune/device-management/tools/deploy-remediations

### Windows Autopilot Enrollment Status Page

Autopilot ESP is a good UX precedent for minimal user involvement. It shows provisioning progress and can block use until required apps and policies are installed. It also supports pre-provisioning/technician flows so much of setup happens before the end user needs the device.

Sources:

- https://learn.microsoft.com/en-us/mem/autopilot/enrollment-status
- https://learn.microsoft.com/en-us/intune/device-enrollment/windows/setup-status-page

### Nexthink Spark

Nexthink Spark is the closest public example to the AI behavior Doxa needs. Its published architecture combines:

- live endpoint context;
- AI-driven diagnosis;
- IT-approved actions;
- end-to-end remediation;
- governance and visibility.

The important lesson is not the chatbot front end. It is that the AI reasons over real endpoint state and executes only organization-approved remediations.

Sources:

- https://docs.nexthink.com/platform/user-guide/spark
- https://nexthink.com/platform/spark

### Tanium, ControlUp and 1E

Current autonomous endpoint-management products converge on the same loop:

- observe endpoint state;
- diagnose/prioritize;
- select governed actions;
- remediate;
- verify continuously.

Tanium emphasizes governed AI-assisted actions and real-time endpoint intelligence. ControlUp describes AI diagnosis plus automated remediation. 1E Endpoint Automation uses policies, rules, triggers and preconditions to enforce device state and self-heal.

Sources:

- https://www.tanium.com/platform/endpoint-management
- https://www.controlup.com/
- https://docs.1e.com/en-2/Content/endpoint-automation/endpoint-automation.htm

### Dell SupportAssist for Business PCs

Dell is especially relevant because it controls much of the supported hardware baseline. SupportAssist for Business PCs provides proactive/predictive hardware and software issue detection, centralized management, telemetry, Dell-authored remediation scripts, custom workflows and Intune deployment guidance.

That is much closer to Doxa than a generic Windows "fix any PC" assistant.

Sources:

- https://www.dell.com/support/contents/en-us/article/product-support/self-support-knowledgebase/software-and-downloads/support-assist/SupportAssist-for-business-pc
- https://www.dell.com/support/manuals/en-uk/supportassist-business-pcs/sab_winos_dg/introduction

## Open-source / standard components we should reuse

### Velopack MSI - use it

Velopack can already generate a Windows MSI, including `PerMachine` installation into Program Files. That gives Doxa a standard enterprise artifact without creating a separate WiX installer project.

Source:

- https://docs.velopack.io/packaging/installer

Repository proof on 2026-09-25:

- `package.ps1 -EnterpriseMsi` produced `Standivarius.Visual-alpha.msi`;
- Windows Installer metadata reports `ALLUSERS=1`;
- ProductName `Visual Alpha`;
- ProductVersion `0.1.0.0`.

### PSAppDeployToolkit - optional enterprise wrapper

PSAppDeployToolkit 4.1.x is an open-source, enterprise-focused wrapper for vendor MSI/EXE installers. It already integrates with Intune and Configuration Manager, supports silent deployments, consistent logging, reboot behavior and enterprise UI conventions.

Doxa should **not require** PSADT for every customer. The MSI should work directly. But for organizations that standardize packaging through PSADT, we should provide a tested recipe/template rather than inventing our own wrapper.

Sources:

- https://github.com/PSAppDeployToolkit/PSAppDeployToolkit
- https://psappdeploytoolkit.com/docs/4.1.x/introduction
- https://psappdeploytoolkit.com/docs/how-to/deploy-with-intune
- https://psappdeploytoolkit.com/docs/how-to/deploy-with-configmgr

### DSC / WinGet Configuration - do not add yet

DSC v3 is open source and provides declarative, idempotent desired-state management. WinGet Configuration uses DSC for repeatable machine setup.

It is technically attractive, but adding it to the Doxa installer now would create another runtime and abstraction layer for a small, controlled hardware stack. Doxa should borrow the **idempotent test/apply/test idea** without introducing DSC until real deployment requirements justify it.

Sources:

- https://github.com/PowerShell/DSC
- https://learn.microsoft.com/en-us/windows/package-manager/configuration/

### osquery / Fleet / Rudder / WAPT - not needed in the client

These projects prove that inventory, policy and endpoint state management can be standardized, but adding a general-purpose endpoint agent to every Doxa installation would be excessive. Doxa already has a narrow known baseline and an existing diagnostics executable. Use native Windows APIs and the existing diagnostics path unless field evidence shows a gap that one of these projects solves better.

## Enterprise policy compatibility

### Code signing is a requirement, not optional polish

Enterprise App Control for Business can allow signed publisher/file rules and can trust software deployed through managed installers such as Intune/Configuration Manager. Microsoft explicitly recommends signing application binaries and scripts.

For Doxa enterprise deployment, signing should cover the installer, application executables, DLLs, helper tools and any scripts we ship.

Sources:

- https://learn.microsoft.com/en-us/windows/security/application-security/application-control/app-control-for-business/design/configure-authorized-apps-deployed-with-a-managed-installer
- https://learn.microsoft.com/en-us/windows/security/application-security/application-control/app-control-for-business/deployment/use-code-signing-for-better-control-and-protection

### Do not bypass enterprise controls

If App Control, proxy policy, privilege policy or update policy prevents a Doxa operation, the correct result is a structured IT-facing block reason. The product should never disable or weaken those controls.

## Adapted Doxa architecture

The research supports a simpler architecture than the earlier generalized plan.

### Normal path

```text
IT / technician deploys Doxa package
        |
        v
standard enterprise installer path
(Velopack per-machine MSI; optional PSADT wrapper)
        |
        v
small deterministic Doxa preflight
        |
        +-- baseline valid --> install
        |
        +-- known local issue --> approved deterministic action
        |
        +-- enterprise policy block --> stop + IT result
        |
        +-- ambiguous exception --> Doxa cloud planner
                                      |
                                      v
                                  Dify / Muse
                                      |
                                      v
                         one approved diagnostic action
        |
        v
post-install health check
        |
        +-- pass --> done
        +-- fail --> one bounded retry/diagnostic or IT escalation
```

### What we deliberately do not build

- no custom enterprise software distribution system;
- no custom MDM agent;
- no arbitrary AI shell executor;
- no local LLM;
- no large generic hardware inventory framework;
- no custom reboot/resume engine for enterprise mode;
- no mandatory PSADT dependency;
- no DSC layer in version 1.

## Important simplification: enterprise systems own reboot/retry

For managed deployment, Doxa should return standard installer results such as success, retry and reboot-required. Intune/ConfigMgr/PSADT already know how to handle those. We should not build another persistent orchestration service unless later evidence proves it necessary.

The Doxa-specific checks should be idempotent so rerunning setup after a reboot simply continues from current machine state.

## Important simplification: AI is diagnostic-only in version 1

The first AI-assisted Doxa setup should allow Muse to choose only read-only diagnostics or escalation actions. Known mutating operations such as installing an approved signed Doxa driver remain deterministic local rules.

This gives us most of the value of AI exception handling while making enterprise review much easier.

## Biggest architectural risk discovered

Enterprise installation commonly runs under SYSTEM/session 0, while Visual/Doxa display and capture behavior ultimately matters in the interactive user session.

Therefore machine installation health and interactive graphics health must be separate:

1. SYSTEM-context setup verifies files, services, driver/PnP state and machine-level prerequisites.
2. A signed user-session health check verifies the actual display/capture behavior at first launch or through an IT-approved user-session invocation.

Trying to prove WGC/interactive magnification health entirely from SYSTEM context would give misleading results.

## Recommended first production shape

- machine package: signed Velopack per-machine MSI;
- enterprise wrapper: direct Intune/ConfigMgr deployment by default; optional PSADT recipe;
- local Doxa setup logic: small signed executable, not a PowerShell framework;
- baseline data: versioned Doxa hardware/software manifest;
- machine diagnostics: extend existing `visual_diagnostics.exe` or successor;
- AI planner: Doxa HTTPS service -> Dify/Muse;
- AI v1 actions: diagnostics/escalation only;
- user-session validation: first-launch health check;
- updates: IT-managed mode must be supported; no forced self-update in enterprise mode.
