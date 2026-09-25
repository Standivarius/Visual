param(
    [Parameter(Mandatory=$true)][string]$Scenario,
    [switch]$UseMuse,
    [string]$Model='muse-spark-1.3-contributor'
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest

function Read-Json([string]$Path){
    if(-not(Test-Path $Path -PathType Leaf)){throw "File not found: $Path"}
    return Get-Content $Path -Raw | ConvertFrom-Json
}

function New-Decision([string]$Outcome,[string]$Action,[string]$Reason,[bool]$UsedAi=$false,[string]$Confidence='deterministic'){
    [pscustomobject]@{
        outcome=$Outcome
        action=$Action
        reason=$Reason
        used_ai=$UsedAi
        confidence=$Confidence
    }
}

function Resolve-Local($s){
    if(-not$s.windows_supported){
        return New-Decision 'blocked_unsupported_os' 'escalate_it' 'The Windows baseline is outside the approved Doxa support matrix.'
    }
    if($s.doxa_hardware -ne 'expected'){
        return New-Decision 'blocked_hardware_baseline' 'recheck_doxa_devices' 'The expected Doxa hardware identity is not present.'
    }
    if($s.enterprise_managed -and $s.deployment_context -ne 'system' -and -not$s.user_is_admin){
        return New-Decision 'it_deployment_required' 'escalate_it' 'The device is enterprise-managed and the current user cannot perform machine-wide installation.'
    }
    if($s.app_control -eq 'block'){
        return New-Decision 'blocked_by_enterprise_policy' 'escalate_it' 'Windows application control is blocking a Doxa component. Doxa must not bypass the enterprise policy.'
    }
    if($s.pending_reboot){
        return New-Decision 'reboot_required' 'return_3010' 'A reboot is already required before setup can safely continue.'
    }
    $expectedDisplays=2
    if($s.PSObject.Properties.Name -contains 'expected_display_count'){
        $expectedDisplays=[Math]::Max(1,[int]$s.expected_display_count)
    }
    if([int]$s.active_displays -lt $expectedDisplays){
        return New-Decision 'waiting_for_display' 'wait_for_display' "The Doxa baseline expects $expectedDisplays active displays and Windows currently reports fewer."
    }
    if($s.driver_state -in @('missing','outdated')){
        return New-Decision 'local_remediation' 'install_approved_driver' 'The Doxa hardware is present but its approved signed driver package is not at the required baseline.'
    }
    if(-not$s.visual_installed){
        return New-Decision 'ready_to_install' 'install_doxa' 'All deterministic Doxa preflight checks passed.'
    }
    if($s.post_install_health -eq 'pass'){
        return New-Decision 'installed_ok' 'finish' 'Doxa is installed and the bounded post-install health check passed.'
    }
    if($s.network_to_doxa_cloud -ne 'reachable'){
        return New-Decision 'needs_support_offline' 'collect_support_bundle' 'The local baseline is valid but the failure is ambiguous and the Doxa cloud planner is unavailable.'
    }
    return New-Decision 'needs_ai_plan' 'none' 'The deterministic baseline passed, but the remaining failure needs exception diagnosis.'
}

function Invoke-MusePlan($s,$actions,[string]$model){
    $key=$env:META_API_KEY
    if(-not$key){throw 'META_API_KEY is not available for the lab simulation.'}
    $allowed=@($actions.ai_actions | ForEach-Object {$_.id})
    $stateJson=$s | ConvertTo-Json -Depth 8 -Compress
    $allowedJson=$allowed | ConvertTo-Json -Compress
    $prompt=@"
You are the exception planner for a controlled Doxa enterprise installer simulation.
The normal installer is deterministic. You are called only when the deterministic baseline cannot explain the remaining failure.
Choose exactly one action from the allowed list. Do not invent commands, scripts, registry changes, driver replacements, security bypasses, or additional actions.
If enterprise policy is likely involved, choose escalate_it.
If evidence is insufficient, prefer a diagnostic action or collect_support_bundle.

Machine state:
$stateJson

Allowed actions:
$allowedJson

Return JSON only with this exact shape:
{"decision":"diagnose|escalate","action":"one allowed action id","reason":"short plain-English reason","confidence":"low|medium|high","needs_it":true|false}
"@
    $headers=@{Authorization="Bearer $key";'Content-Type'='application/json'}
    $body=@{model=$model;messages=@(@{role='user';content=$prompt});temperature=0}|ConvertTo-Json -Depth 8
    $r=Invoke-RestMethod -Method Post -Uri 'https://api.meta.ai/v1/chat/completions' -Headers $headers -Body $body -TimeoutSec 90
    $text=[string]$r.choices[0].message.content
    $text=$text.Trim()
    if($text.StartsWith('```')){
        $text=$text -replace '^```(?:json)?\s*','' -replace '\s*```$',''
    }
    try{$plan=$text|ConvertFrom-Json}catch{throw "Muse returned non-JSON output: $text"}
    if($allowed -notcontains [string]$plan.action){throw "Muse selected an action outside the allowlist: $($plan.action)"}
    return [pscustomobject]@{
        outcome='ai_plan'
        action=[string]$plan.action
        reason=[string]$plan.reason
        used_ai=$true
        confidence=[string]$plan.confidence
        needs_it=[bool]$plan.needs_it
        model=$model
    }
}

$scenarioObj=Read-Json ([IO.Path]::GetFullPath($Scenario))
$actions=Read-Json (Join-Path $PSScriptRoot 'approved-actions.json')
$decision=Resolve-Local $scenarioObj.state
if($decision.outcome -eq 'needs_ai_plan' -and $UseMuse){
    try{
        $decision=Invoke-MusePlan $scenarioObj.state $actions $Model
    }catch{
        $decision=[pscustomobject]@{
            outcome='ai_plan_failed'
            action='collect_support_bundle'
            reason="Cloud planning failed safely: $($_.Exception.Message)"
            used_ai=$true
            confidence='fallback'
            needs_it=$false
            model=$Model
        }
    }
}

[pscustomobject]@{
    scenario_id=$scenarioObj.id
    description=$scenarioObj.description
    expected_outcome=$scenarioObj.expected_outcome
    decision=$decision
    pass=([string]$scenarioObj.expected_outcome -eq [string]$decision.outcome -or ($scenarioObj.expected_outcome -eq 'ai_plan' -and $decision.outcome -in @('needs_ai_plan','ai_plan')))
} | ConvertTo-Json -Depth 10
