param(
    [string]$ConfigPath=(Join-Path $PSScriptRoot 'pilot-config.json'),
    [switch]$ExercisePlanner,
    [switch]$SkipInstall
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest

function Read-Json([string]$Path){
    if(-not(Test-Path $Path -PathType Leaf)){throw "File not found: $Path"}
    Get-Content $Path -Raw|ConvertFrom-Json
}
function Test-PendingReboot {
    foreach($path in @(
        'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Component Based Servicing\RebootPending',
        'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\WindowsUpdate\Auto Update\RebootRequired')){
        if(Test-Path $path){return $true}
    }

    return $false
}
function Get-InstalledAppDir {
    $candidate=Join-Path $env:LOCALAPPDATA 'Standivarius.Visual\current'
    if(Test-Path (Join-Path $candidate 'visual_app.exe') -PathType Leaf){return $candidate}
    $entry=Get-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\Standivarius.Visual' -ErrorAction SilentlyContinue
    if($entry -and $entry.InstallLocation){
        $candidate=Join-Path ([string]$entry.InstallLocation) 'current'
        if(Test-Path (Join-Path $candidate 'visual_app.exe') -PathType Leaf){return $candidate}
    }
    return $null
}
function Invoke-Health([string]$VisualExe,[string]$Telemetry){
    $p=Start-Process -FilePath $VisualExe -ArgumentList @('--health-check','--health-timeout-ms','7000','--zoom','2','--log',$Telemetry) -Wait -PassThru
    [pscustomobject]@{exit_code=$p.ExitCode;result=if($p.ExitCode-eq0){'pass'}else{'fail'};telemetry=$Telemetry}
}
function New-SupportBundle([string]$RunDir){
    $zip=Join-Path $RunDir 'doxa-pilot-support.zip'
    $items=Get-ChildItem $RunDir -File | Where-Object {$_.FullName-ne$zip}
    if($items){Compress-Archive -Path $items.FullName -DestinationPath $zip -Force}
    return $zip
}
function Test-Planner([string]$Url){
    if([string]::IsNullOrWhiteSpace($Url)){return $false}
    try{
        $r=Invoke-RestMethod -Method Get -Uri ($Url.TrimEnd('/')+'/health') -TimeoutSec 10
        return [bool]$r.ok
    }catch{return $false}
}

$config=Read-Json ([IO.Path]::GetFullPath($ConfigPath))
$installer=Join-Path $PSScriptRoot ([string]$config.installer_file)
$simulate=Join-Path $PSScriptRoot 'simulate.ps1'
$actionsPath=Join-Path $PSScriptRoot 'approved-actions.json'
foreach($required in @($simulate,$actionsPath)){if(-not(Test-Path $required -PathType Leaf)){throw "Pilot component missing: $required"}}

$stateRoot=Join-Path $env:LOCALAPPDATA 'Doxa\Pilot'
$runDir=Join-Path $stateRoot ((Get-Date -Format 'yyyy-MM-dd_HH-mm-ss')+'__pilot')
New-Item -ItemType Directory -Force -Path $runDir|Out-Null
$installLog=Join-Path $runDir 'install.log'

$appDir=Get-InstalledAppDir
$alreadyCurrent=$false
if($appDir -and $config.PSObject.Properties.Name -contains 'visual_version'){
    $releaseInfo=Join-Path $appDir 'release-info.txt'
    if(Test-Path $releaseInfo -PathType Leaf){
        $installedLine=Get-Content $releaseInfo|Where-Object{$_ -like 'visual_version=*'}|Select-Object -First 1
        if($installedLine -and $installedLine.Substring('visual_version='.Length)-eq[string]$config.visual_version){$alreadyCurrent=$true}
    }
}
if(-not$SkipInstall -and -not$alreadyCurrent){
    if(-not(Test-Path $installer -PathType Leaf)){throw "Installer not found: $installer"}
    Write-Host 'Installing Visual/Doxa pilot...'
    $setup=Start-Process -FilePath $installer -ArgumentList @('--silent','--log',$installLog) -Wait -PassThru
    if($setup.ExitCode-ne0){
        $failure=[pscustomobject]@{schema_version='1';stage='install';outcome='installer_failed';exit_code=$setup.ExitCode;install_log=$installLog}
        $failure|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 (Join-Path $runDir 'result.json')
        New-SupportBundle $runDir|Out-Null
        throw "Installer failed with exit code $($setup.ExitCode). Evidence: $runDir"
    }
    $deadline=(Get-Date).AddSeconds(30)
    do{$appDir=Get-InstalledAppDir;if($appDir){break};Start-Sleep -Milliseconds 250}while((Get-Date)-lt$deadline)
}elseif($alreadyCurrent){
    Write-Host "Visual/Doxa pilot $($config.visual_version) is already installed; reusing it."
}
if(-not$appDir){throw 'Visual installation was not found after setup.'}

# Velopack may launch the normal app after setup. Stop only Visual before the bounded probe.
Get-Process visual_app -ErrorAction SilentlyContinue|Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 250

$visualExe=Join-Path $appDir 'visual_app.exe'
$diagnosticsExe=Join-Path $appDir 'visual_diagnostics.exe'
foreach($required in @($visualExe,$diagnosticsExe)){if(-not(Test-Path $required -PathType Leaf)){throw "Installed component missing: $required"}}

$diagnosticsPath=Join-Path $runDir 'diagnostics.json'
& $diagnosticsExe --out $diagnosticsPath
if($LASTEXITCODE-ne0){throw "Diagnostics failed: $LASTEXITCODE"}
$diagnostics=Read-Json $diagnosticsPath
$health=Invoke-Health $visualExe (Join-Path $runDir 'health.csv')

$identity=[Security.Principal.WindowsIdentity]::GetCurrent()
$principal=New-Object Security.Principal.WindowsPrincipal($identity)
$isAdmin=$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
$plannerReachable=Test-Planner ([string]$config.planner_url)
$expected=if($config.PSObject.Properties.Name -contains 'expected_display_count'){[Math]::Max(1,[int]$config.expected_display_count)}else{2}
$state=[pscustomobject]@{
    mode='visual_two_monitor_pilot'
    doxa_hardware='expected'
    hardware_profile='surrogate-two-monitor'
    expected_display_count=$expected
    windows_supported=([int]$diagnostics.windows.major -gt 10 -or ([int]$diagnostics.windows.major -eq 10 -and [int]$diagnostics.windows.build -ge 19041))
    windows_build=[int]$diagnostics.windows.build
    enterprise_managed=$false
    deployment_context='user'
    user_is_admin=[bool]$isAdmin
    active_displays=[int]$diagnostics.active_monitor_count
    driver_state='approved'
    driver_version='surrogate-host-stack'
    firmware_version='not-applicable-surrogate'
    pending_reboot=(Test-PendingReboot)
    app_control='allow'
    network_to_doxa_cloud=if($plannerReachable){'reachable'}else{'blocked'}
    wgc_health=$health.result
    visual_installed=$true
    visual_version=[string]$diagnostics.visual_version
    post_install_health=$health.result
    actual_post_install_health=$health.result
    planner_test_injected=$false
}

if($ExercisePlanner -and $health.result-eq'pass'){
    # Exercise the exception path without damaging a healthy test PC. The real
    # health result is retained separately and the follow-up action is real.
    $state.post_install_health='fail'
    $state.wgc_health='fail'
    $state.planner_test_injected=$true
}

$scenario=[pscustomobject]@{id='visual_two_monitor_pilot';description='Portable two-monitor Doxa lifecycle pilot.';expected_outcome='auto';state=$state}
$scenarioPath=Join-Path $runDir 'planner-input.json'
$scenario|ConvertTo-Json -Depth 10|Set-Content -Encoding UTF8 $scenarioPath
$plannerToken=[string]$config.planner_token
$plannerUrl=[string]$config.planner_url
$clientId='pilot-'+[guid]::NewGuid().ToString('N').Substring(0,12)
$plannerJson=& $simulate -Scenario $scenarioPath -PlannerUrl $plannerUrl -PlannerToken $plannerToken -ClientId $clientId
if($LASTEXITCODE-ne0){throw "Planner decision failed: $LASTEXITCODE"}
$planner=$plannerJson|ConvertFrom-Json

$followUp=$null
switch([string]$planner.decision.action){
    'run_minimal_capture_probe' {$followUp=Invoke-Health $visualExe (Join-Path $runDir 'followup-health.csv')}
    'retry_post_install_health' {$followUp=Invoke-Health $visualExe (Join-Path $runDir 'followup-health.csv')}
    'recheck_doxa_devices' {
        $followPath=Join-Path $runDir 'followup-diagnostics.json'
        & $diagnosticsExe --out $followPath
        $followUp=[pscustomobject]@{exit_code=$LASTEXITCODE;result=if($LASTEXITCODE-eq0){'collected'}else{'failed'};diagnostics=$followPath}
    }
}

$supportBundle=$null
if($planner.decision.action-eq'collect_support_bundle' -or $planner.decision.outcome-eq'ai_plan_failed'){$supportBundle=New-SupportBundle $runDir}
$result=[pscustomobject]@{
    schema_version='1'
    run_dir=$runDir
    state=$state
    initial_health=$health
    decision=$planner.decision
    follow_up=$followUp
    support_bundle=$supportBundle
}
$resultPath=Join-Path $runDir 'result.json'
$result|ConvertTo-Json -Depth 12|Set-Content -Encoding UTF8 $resultPath

Write-Host ''
Write-Host 'Doxa pilot result'
Write-Host "  Displays: $($state.active_displays) / expected $expected"
Write-Host "  Initial graphics health: $($health.result)"
Write-Host "  Decision: $($planner.decision.outcome) / $($planner.decision.action)"
if($planner.decision.used_ai){Write-Host "  Cloud planner used: yes ($($planner.decision.provider))"}
if($followUp){Write-Host "  Follow-up: $($followUp.result)"}
Write-Host "  Evidence: $resultPath"

$success=$false
if($planner.decision.outcome-eq'installed_ok'){$success=$true}
elseif($planner.decision.outcome-eq'ai_plan' -and $followUp -and $followUp.result -in @('pass','collected')){$success=$true}
if(-not$success){exit 20}
