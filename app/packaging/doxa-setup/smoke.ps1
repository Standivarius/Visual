param(
    [ValidateSet('Debug','Release','RelWithDebInfo','MinSizeRel')][string]$Configuration='Release',
    [switch]$UseMuse
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest

$runner=Join-Path $PSScriptRoot 'run-surrogate.ps1'
if(-not(Test-Path $runner -PathType Leaf)){throw "Runner not found: $runner"}
$repoRoot=Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
$outRoot=Join-Path $repoRoot 'artifacts\doxa-surrogate-smoke'
New-Item -ItemType Directory -Force -Path $outRoot|Out-Null
$override=Join-Path $outRoot 'ignore-host-pending-reboot-for-simulation.json'
'{"pending_reboot":false}'|Set-Content -Encoding UTF8 $override

$results=@()
$happy=& $runner -Configuration $Configuration -HealthMode actual -StateOverride $override -OutputDir $outRoot -PassThru
$results+=[pscustomobject]@{case='live_two_monitor_health';expected='installed_ok';actual=$happy.outcome;pass=($happy.outcome-eq'installed_ok');detail=$happy.result_json}

$missing=& $runner -Configuration $Configuration -HealthMode not-run -ExpectedDisplayCount ([int]$happy.active_displays+1) -StateOverride $override -OutputDir $outRoot -PassThru
$results+=[pscustomobject]@{case='future_doxa_missing_display';expected='waiting_for_display';actual=$missing.outcome;pass=($missing.outcome-eq'waiting_for_display');detail=$missing.result_json}

$offline=& $runner -Configuration $Configuration -HealthMode fail -StateOverride $override -CloudBlocked -OutputDir $outRoot -PassThru
$offlineDetail=Get-Content $offline.result_json -Raw|ConvertFrom-Json
$offlinePass=($offline.outcome-eq'needs_support_offline' -and $offline.action-eq'collect_support_bundle' -and $offlineDetail.support_bundle -and (Test-Path $offlineDetail.support_bundle))
$results+=[pscustomobject]@{case='ambiguous_failure_cloud_blocked';expected='needs_support_offline+bundle';actual="$($offline.outcome)+$($offline.action)";pass=$offlinePass;detail=$offline.result_json}

if($UseMuse){
    $ai=& $runner -Configuration $Configuration -HealthMode fail -StateOverride $override -UseMuse -OutputDir $outRoot -PassThru
    $aiPass=($ai.outcome-eq'ai_plan' -and $ai.action -in @('run_minimal_capture_probe','retry_post_install_health','recheck_doxa_devices','collect_support_bundle','escalate_it'))
    if($ai.action -in @('run_minimal_capture_probe','retry_post_install_health')){$aiPass=$aiPass -and $ai.follow_up -eq'pass'}
    $results+=[pscustomobject]@{case='ambiguous_failure_muse';expected='allowlisted_ai_plan';actual="$($ai.outcome)+$($ai.action)+followup=$($ai.follow_up)";pass=$aiPass;detail=$ai.result_json}
}

$visualExe=Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) "build\$Configuration\visual_app.exe"
$priorUpdateMode=$env:VISUAL_UPDATE_MODE
try{
    $env:VISUAL_UPDATE_MODE='it-managed'
    $policyProcess=Start-Process -FilePath $visualExe -ArgumentList '--update-check' -Wait -PassThru
}finally{
    if($null-eq$priorUpdateMode){Remove-Item Env:VISUAL_UPDATE_MODE -ErrorAction SilentlyContinue}else{$env:VISUAL_UPDATE_MODE=$priorUpdateMode}
}
$results+=[pscustomobject]@{case='it_managed_update_policy';expected='exit_43_no_update_contact';actual="exit_$($policyProcess.ExitCode)";pass=($policyProcess.ExitCode-eq43);detail=(Join-Path $env:LOCALAPPDATA 'Standivarius.Visual\visual_update.log')}

$stamp=Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
$summaryPath=Join-Path $outRoot "${stamp}__summary.json"
$results|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 $summaryPath
$results|Format-Table -AutoSize
"summary=$summaryPath"
if(@($results|Where-Object{-not$_.pass}).Count){exit 1}
