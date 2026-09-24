# Doxa installation and support architecture

## Objective

Install Doxa with almost no end-user involvement while fitting cleanly into enterprise IT controls.

Doxa is a controlled hardware/software product, not a generic Windows application expected to repair arbitrary PCs. The normal installation path should therefore be deterministic. AI is an exception planner, not the installer.

## Final v1 architecture

```text
Enterprise IT / Doxa technician
        |
        v
Velopack per-machine MSI
(direct Intune/ConfigMgr, optional PSAppDeployToolkit wrapper)
        |
        v
Doxa preflight
        |
        +-- known baseline state --------> deterministic action
        |
        +-- enterprise policy block ----> machine-readable IT result
        |
        +-- ambiguous exception --------> Doxa cloud -> Dify/Muse
                                             |
                                             v
                                    approved diagnostic action
        |
        v
install / verify machine state
        |
        v
interactive user-session health check
        |
        +-- pass --> ready
        +-- fail --> bounded diagnosis / IT escalation
```

## What belongs to existing tools

### Velopack

Owns Visual/Doxa application packaging and update layout. For enterprise deployment generate a per-machine MSI.

### Intune / Configuration Manager

Own deployment targeting, SYSTEM context, dependencies, detection rules, retries, reporting and enterprise reboot policy.

### PSAppDeployToolkit

Optional wrapper for customers whose application-packaging practice standardizes on PSADT. Use its existing logging, silent mode and enterprise UI instead of creating our own wrapper.

### Windows

Owns UAC, driver signing, App Control, service control, Windows Installer, PnP and enterprise policy enforcement.

### Doxa code

Owns only Doxa-specific baseline checks, Doxa-specific health checks and bounded actions.

## Deterministic baseline

The eventual production baseline manifest should contain only facts Doxa can qualify, for example:

- Doxa hardware/revision identifiers;
- supported Windows build range;
- approved signed driver/firmware versions;
- expected display functions/topology;
- required Doxa software version;
- local health-test expectations.

Unknown values are not guessed. Until representative production Doxa hardware is available, the repository keeps these as placeholders/test scenarios.

## Local decision order

1. Is the Windows build supported?
2. Is expected Doxa hardware present?
3. Is this an enterprise-managed device running under an approved deployment context?
4. Is enterprise application control blocking a Doxa component?
5. Is a reboot already required?
6. Is the expected Doxa display topology present?
7. Are approved signed Doxa drivers present?
8. Is Doxa installed?
9. Does the machine-level health check pass?
10. If the remaining failure is ambiguous, call the cloud exception planner.

Known states should not consume an AI call.

## AI boundary - version 1

Muse may choose only these diagnostic/escalation actions:

- `run_minimal_capture_probe`;
- `recheck_doxa_devices`;
- `retry_post_install_health`;
- `collect_support_bundle`;
- `escalate_it`.

Muse may not install drivers, edit policy, run arbitrary PowerShell, change security settings or invent remediation commands.

Known mutating actions remain deterministic local code. Example: if a known Doxa hardware revision has a missing approved signed driver and policy permits installation, setup can install that exact package without consulting Muse.

## Enterprise behavior

### Restricted user rights

Do not prompt the employee for administrator credentials. Return an IT-deployment-required result unless setup is already running under the enterprise deployment service/SYSTEM context.

### Application control

If App Control/AppLocker blocks a Doxa binary, stop and return the publisher/file/version evidence needed by IT. Never disable the policy.

### Proxy/cloud restrictions

Normal installation must not depend on Dify/Muse. If the cloud exception planner is blocked and the local state is ambiguous, create a support bundle and return an IT/support result.

### Updates

Support both Doxa-managed and IT-managed update policy. Enterprise-managed installations must be able to suppress application-driven updates so IT can deploy approved versions through its normal channel.

## Reboot and retry

Do not build a custom enterprise reboot scheduler in version 1.

Return standard reboot/retry outcomes and make the Doxa checks idempotent. Let Intune/ConfigMgr/PSADT handle the enterprise reboot/retry lifecycle.

## Interactive graphics health

This is a separate phase from machine installation.

SYSTEM/session-0 installation can verify machine state, files, drivers and PnP. It cannot be treated as authoritative proof of WGC/capture behavior in the user's interactive session.

The final product should run a short signed user-session health check at first launch (or another IT-approved user-session trigger) and record a simple pass/fail/support code.

## User experience

For a normal enterprise deployment the employee should not see a wizard.

For technician/standalone deployment, the UI should show only meaningful states such as:

- Checking Doxa hardware
- Preparing displays
- Installing Doxa
- A restart is required
- Waiting for the second Doxa display
- Your organization must approve Doxa before setup can continue
- Doxa is ready

No chat interface is part of the installation product.

## Current prototype

`app/packaging/doxa-setup/` contains a deliberately small decision simulator. It demonstrates:

- deterministic handling of normal/known states;
- enterprise-policy escalation;
- offline-safe failure behavior;
- a Muse Contributor call only for an ambiguous failure;
- an AI allowlist containing only diagnostic/escalation actions.

`app/packaging/package.ps1 -EnterpriseMsi` now asks the existing pinned Velopack toolchain to create a per-machine MSI. This is the preferred enterprise package foundation; no separate WiX project was added.

## Next implementation milestone

Do not add more orchestration frameworks yet.

The next useful code milestone is a small signed `DoxaSetup`/preflight executable that consumes the real Doxa baseline once production hardware identifiers and approved driver packages are known. Until then, use the simulation contract to validate enterprise scenarios and the Dify/Muse planner behavior.
