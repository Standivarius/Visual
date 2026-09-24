# Doxa setup decision prototype

This is intentionally small. It is not a second installer framework.

It models only the Doxa-specific decisions that sit around established deployment tooling:

- Velopack produces the application package and per-machine MSI;
- enterprise IT may deploy that MSI directly or wrap it with PSAppDeployToolkit for Intune/ConfigMgr conventions;
- Windows/enterprise policy remains authoritative;
- the Doxa layer checks the known Doxa baseline and post-install health;
- Muse is called only for ambiguous exceptions and can select only read-only diagnostic/escalation actions in version 1.

Run all deterministic scenarios:

```powershell
Get-ChildItem .\app\packaging\doxa-setup\scenarios\*.json | ForEach-Object {
    .\app\packaging\doxa-setup\simulate.ps1 -Scenario $_.FullName
}
```

Run the ambiguous case with the lab Meta credential already present in the environment:

```powershell
.\app\packaging\doxa-setup\simulate.ps1 `
  -Scenario .\app\packaging\doxa-setup\scenarios\07-ambiguous-capture-failure.json `
  -UseMuse
```

The direct Meta call exists only for this repository simulation. Production Doxa clients should call a Doxa-owned HTTPS service, which can in turn call Dify/Muse. No model-provider key belongs in the client package.
