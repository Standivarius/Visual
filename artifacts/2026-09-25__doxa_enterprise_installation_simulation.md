# Doxa enterprise installation simulation and risk review

**Date:** 2026-09-25
**Status:** architecture simulation; no production Doxa hardware assumptions were invented.

## What was built for the simulation

- `app/packaging/package.ps1 -EnterpriseMsi` now generates a Velopack per-machine MSI.
- `app/packaging/doxa-setup/simulate.ps1` models only Doxa-specific decisions.
- `app/packaging/doxa-setup/approved-actions.json` limits Muse version 1 to diagnostic/escalation actions.
- nine synthetic enterprise scenarios exercise the proposed flow.
- the ambiguous scenario can call `muse-spark-1.3-contributor` with synthetic machine state.

The direct Meta call in the simulator is lab-only. Production architecture remains Doxa client -> Doxa HTTPS service -> Dify/Muse.

## Packaging proof

Command used:

```powershell
.\app\packaging\package.ps1 `
  -Version 0.1.0-alpha.3 `
  -Configuration Release `
  -SkipBuild `
  -EnterpriseMsi `
  -OutputDir .\app\packaging\out-enterprise-sim
```

Result:

- Velopack 1.2.0 completed successfully;
- `Standivarius.Visual-alpha.msi` created;
- MSI size: 6,758,400 bytes in this run;
- Windows Installer metadata: `ALLUSERS=1`;
- Manufacturer: `Standivarius`;
- ProductName: `Visual Alpha`;
- ProductVersion: `0.1.0.0`.

This proves we do not need a second hand-written WiX installer project merely to obtain an enterprise MSI.

The package is still unsigned. That is acceptable for this engineering proof and is **not** acceptable for an enterprise pilot.

## Deterministic scenario results

| Scenario | Expected | Actual | Local action | Result |
| --- | --- | --- | --- | --- |
| managed machine, ready | `ready_to_install` | `ready_to_install` | `install_doxa` | pass |
| already installed and healthy | `installed_ok` | `installed_ok` | `finish` | pass |
| restricted employee launches setup | `it_deployment_required` | `it_deployment_required` | `escalate_it` | pass |
| App Control blocks Doxa | `blocked_by_enterprise_policy` | `blocked_by_enterprise_policy` | `escalate_it` | pass |
| second display absent | `waiting_for_display` | `waiting_for_display` | `wait_for_display` | pass |
| approved driver missing | `local_remediation` | `local_remediation` | `install_approved_driver` | pass |
| ambiguous capture failure | `ai_plan` | `needs_ai_plan` locally | no local guess | pass |
| Doxa cloud blocked during ambiguous failure | `needs_support_offline` | `needs_support_offline` | `collect_support_bundle` | pass |
| pending reboot | `reboot_required` | `reboot_required` | `return_3010` | pass |

All nine deterministic scenario checks passed.

## Muse exception simulation

Synthetic state:

- expected Doxa hardware present;
- supported Windows baseline;
- enterprise deployment under SYSTEM;
- two active displays;
- approved driver present;
- no App Control block;
- Doxa cloud reachable;
- Visual installed;
- WGC health failed;
- post-install health failed.

Allowed AI actions were deliberately limited to:

- `run_minimal_capture_probe`;
- `recheck_doxa_devices`;
- `retry_post_install_health`;
- `collect_support_bundle`;
- `escalate_it`.

Three consecutive `muse-spark-1.3-contributor` runs selected:

```text
run_minimal_capture_probe
run_minimal_capture_probe
run_minimal_capture_probe
```

The wording/confidence varied slightly, but the action did not. Because the action is read-only, planner variance at this level has limited blast radius.

## Walkthrough 1 - normal enterprise deployment

1. IT assigns the Doxa Win32/MSI package to the device.
2. Intune/ConfigMgr runs installation under SYSTEM; the employee does not need admin rights.
3. Doxa preflight checks the known hardware/software baseline.
4. All checks pass.
5. The per-machine MSI installs Doxa to Program Files.
6. Machine-level verification checks files, package state, driver/PnP state and prerequisites.
7. Installation reports success to enterprise management.
8. On first interactive Doxa launch, a short user-session graphics/capture health check runs.
9. If that passes, the user sees Doxa ready. No AI call was needed.

This should be the dominant production path.

## Walkthrough 2 - restricted employee starts setup manually

1. Doxa recognizes an enterprise-managed machine and non-elevated user context.
2. It does not ask the employee to find an administrator password.
3. It reports `it_deployment_required` with package/publisher/version information for IT.
4. IT deploys the same package under its existing software-management system.

User involvement is minimal and Doxa does not compete with enterprise privilege policy.

## Walkthrough 3 - App Control blocks Doxa

1. Package/files are otherwise correct.
2. A Doxa executable is blocked by App Control/AppLocker.
3. Doxa records the blocked filename, publisher/signing identity, version and relevant Windows event evidence.
4. It returns `blocked_by_enterprise_policy`.
5. No AI or local action is allowed to disable the policy.
6. IT can allow the Doxa publisher/file through its normal App Control process or managed-installer policy.

## Walkthrough 4 - known Doxa driver missing

1. Known Doxa hardware identity is present.
2. The approved driver baseline is missing/outdated.
3. This is not an AI problem.
4. SYSTEM-context setup installs the exact qualified, signed Doxa/vendor package if enterprise policy permits it.
5. If the package requires reboot, setup returns the standard reboot-required result.
6. Enterprise deployment tooling controls the restart/re-evaluation.

Production implementation remains blocked on authoritative hardware IDs, redistribution rights and approved driver packages from the Doxa hardware/OEM stack.

## Walkthrough 5 - ambiguous graphics failure

1. Hardware, driver, displays and policy all match the approved baseline.
2. Post-install user-session graphics health fails.
3. The structured state is sent to the Doxa cloud planner.
4. Muse receives only the structured evidence plus the approved action list.
5. In the simulation Muse selects `run_minimal_capture_probe`.
6. The signed local Doxa probe runs and produces new structured evidence.
7. The next decision is based on that evidence.
8. If still ambiguous, collect support evidence or escalate rather than expanding local authority.

## Walkthrough 6 - enterprise network blocks Doxa cloud

1. Local deterministic installation still works.
2. Only an ambiguous exception requires cloud reasoning.
3. If the Doxa endpoint is blocked by proxy/firewall policy, setup does not ask the user to change proxy/security settings.
4. It creates the approved local support bundle and returns an IT-facing result.
5. IT can decide whether to allow the documented Doxa HTTPS endpoint.

## Riskiest areas and how to solve them

### 1. SYSTEM install versus interactive graphics validation - highest technical risk

**Risk:** Enterprise software is normally installed under SYSTEM/session 0. WGC and the actual low-vision desktop experience matter in the interactive user session. A session-0 installer cannot be treated as authoritative proof that Doxa magnification works.

**Plan:** Split verification in two. Machine setup validates package/driver/PnP prerequisites. A small signed first-launch user-session health test validates display/capture behavior. Do not add a Windows service merely to bridge sessions unless testing later proves it necessary.

### 2. Code signing and App Control - highest enterprise-adoption risk

**Risk:** Current alpha binaries are unsigned. Many enterprise environments will block or distrust them.

**Plan:** Sign every executable/DLL/installer shipped in the Doxa package before an enterprise pilot. Publish the signing identity and version/publisher information in the deployment pack. Support IT-managed installer trust/publisher rules. Never bypass App Control.

### 3. Driver/firmware ownership - high external dependency

**Risk:** The exact production Doxa controller, driver path, firmware updater and redistribution rights are not yet authoritative in the repository.

**Plan:** Do not automate driver/firmware changes from inference. Obtain the OEM/manufacturer package, supported version matrix, signing and redistribution/update-channel details first. Keep driver installation deterministic and package-specific.

### 4. Enterprise update governance - high acceptance risk

**Risk:** A self-updating desktop app can violate customer change-control policy.

**Plan:** Add an IT-managed update mode before enterprise pilot. In that mode, application-driven update checks/apply are disabled and IT deploys approved MSI versions through its normal channel. Keep Doxa-managed updates as a separate policy choice.

### 5. AI planner error or inconsistency - medium risk in the proposed v1

**Risk:** LLM reasoning can vary or be wrong.

**Plan:** Version 1 allows only read-only diagnostic/escalation actions. Validate the returned action against the compiled allowlist. Unknown/invalid output falls back to `collect_support_bundle`. Three simulation runs returned the same bounded diagnostic action.

### 6. Cloud/proxy/data-governance restrictions - medium/high enterprise risk

**Risk:** Enterprises may block Dify/Meta or reject direct third-party AI traffic.

**Plan:** The Doxa client talks only to a Doxa-owned HTTPS endpoint. The service strips/minimizes payload fields and calls Dify/Muse server-side. Normal installation does not require the cloud. Document the single Doxa endpoint and data fields for IT review.

### 7. Deployment wrapper sprawl - medium complexity risk

**Risk:** Building custom logic for Intune, ConfigMgr, user dialogs, logs, retries and reboots would duplicate mature tooling.

**Plan:** Direct per-machine MSI is the baseline. Offer a PSAppDeployToolkit recipe only for customers who already use that convention. Do not vendor or require PSADT in the Doxa client.

### 8. Reboot/resume complexity - medium risk, deliberately deferred

**Risk:** A custom persistent installer state machine is easy to overbuild.

**Plan:** Make preflight/actions idempotent. Return standard reboot/retry results and let enterprise deployment tools own restart/re-evaluation. Only add a Doxa-specific resume service if real field evidence proves this insufficient.

### 9. Excess collection of enterprise data - medium trust risk

**Risk:** Broad logs/screenshots/user files will slow security approval and create privacy issues.

**Plan:** Send normalized Doxa installation state by default: versions, hardware IDs, result codes and health-test outcomes. No screenshots, user documents, browser contents or arbitrary logs by default.

## Architecture changes made after the research

The earlier plan was simplified in four ways:

1. use Velopack's existing per-machine MSI instead of starting a new WiX installer project;
2. use PSAppDeployToolkit only as an optional enterprise packaging convention, not a product dependency;
3. let enterprise deployment systems own reboot/retry rather than building our own service/state engine;
4. restrict AI version 1 to read-only diagnostics/escalation rather than mutating remediation.

These changes reduce custom Doxa code while preserving the useful AI exception-planning concept.

## Next real-hardware milestone

Before adding more setup code, connect a representative production Doxa unit and obtain the OEM deployment package. Then replace the synthetic baseline with authoritative:

- hardware IDs/revision;
- approved signed driver package/version;
- firmware/update path;
- expected display topology;
- Windows baseline;
- machine-level health probes;
- interactive user-session health criteria.

That is the point at which the simulation should become a real signed `DoxaSetup` preflight executable.
