param(
    [ValidateSet('Debug','Release','RelWithDebInfo','MinSizeRel')][string]$Configuration='Release',
    [ValidateSet('actual','pass','fail','not-run')][string]$HealthMode='actual',
    [int]$ExpectedDisplayCount=2,
    [switch]$EnterpriseManaged,
    [switch]$CloudBlocked,
    [switch]$UseMuse,
    [string]$Model='muse-spark-1.3-contributor',
    [string]$StateOverride='',
    [string]$OutputDir='',
    [switch]$PassThru
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest

$appRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$repoRoot=Split-Path -Parent $appRoot
if(-not$OutputDir){$OutputDir=Join-Path $repoRoot 'artifacts\doxa-surrogate'}
New-Item -ItemType Directory -Force -Path $OutputDir|Out-Null

$diagnosticsExe=Join-Path $appRoot "build\$Configuration\visual_diagnostics.exe"
$visualExe=Join-Path $appRoot "build\$Configuration\visual_app.exe"
$simulate=Join-Path $PSScriptRoot 'simulate.ps1'
$supportBundle=Join-Path (Split-Path -Parent $PSScriptRoot) 'support-bundle.ps1'
foreach($required in @($diagnosticsExe,$visualExe,$simulate,$supportBundle)){
    if(-not(Test-Path $required -PathType Leaf)){throw "Required file not found: $required"}
}

function Test-PendingReboot {
    $checks=@(
        'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Component Based Servicing\RebootPending',
        'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\WindowsUpdate\Auto Update\RebootRequired'
    )
    foreach($path in $checks){if(Test-Path $path){return $true}}

    return $false
}

function Invoke-HealthCheck([string]$TelemetryPath){
    $arguments=@('--health-check','--health-timeout-ms','7000','--zoom','2','--log',$TelemetryPath)
    $process=Start-Process -FilePath $visualExe -ArgumentList $arguments -Wait -PassThru
    [pscustomobject]@{
        exit_code=$process.ExitCode
        result=if($process.ExitCode-eq0){'pass'}else{'fail'}
        telemetry=$TelemetryPath
    }
}

function Set-ObjectProperty($Object,[string]$Name,$Value){
    if($Object.PSObject.Properties.Name -contains $Name){$Object.$Name=$Value}
    else{$Object|Add-Member -NotePropertyName $Name -NotePropertyValue $Value}
}

$stamp=Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
$runDir=Join-Path $OutputDir "${stamp}__visual_surrogate"
New-Item -ItemType Directory -Force -Path $runDir|Out-Null
$diagnosticsPath=Join-Path $runDir 'diagnostics.json'
& $diagnosticsExe --out $diagnosticsPath
if($LASTEXITCODE-ne0){throw "visual_diagnostics.exe failed: $LASTEXITCODE"}
$diagnostics=Get-Content $diagnosticsPath -Raw|ConvertFrom-Json

$identity=[Security.Principal.WindowsIdentity]::GetCurrent()
$principal=New-Object Security.Principal.WindowsPrincipal($identity)
$isSystem=($identity.User.Value -eq 'S-1-5-18')
$isAdmin=$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
$windowsSupported=([int]$diagnostics.windows.major -gt 10 -or ([int]$diagnostics.windows.major -eq 10 -and [int]$diagnostics.windows.build -ge 19041))

$state=[pscustomobject]@{
    mode='visual_two_monitor_surrogate'
    doxa_hardware='expected'
    hardware_profile='surrogate-two-monitor'
    expected_display_count=[Math]::Max(1,$ExpectedDisplayCount)
    windows_supported=$windowsSupported
    windows_build=[int]$diagnostics.windows.build
    enterprise_managed=[bool]$EnterpriseManaged
    deployment_context=if($isSystem){'system'}else{'user'}
    user_is_admin=[bool]$isAdmin
    active_displays=[int]$diagnostics.active_monitor_count
    driver_state='approved'
    driver_version='surrogate-host-stack'
    firmware_version='not-applicable-surrogate'
    pending_reboot=(Test-PendingReboot)
    app_control='allow'
    network_to_doxa_cloud=if($CloudBlocked){'blocked'}else{'reachable'}
    wgc_health='not_run'
    visual_installed=[bool]$diagnostics.visual_app.present
    visual_version=[string]$diagnostics.visual_version
    post_install_health='not_run'
}

if($StateOverride){
    $overridePath=[IO.Path]::GetFullPath($StateOverride)
    if(-not(Test-Path $overridePath -PathType Leaf)){throw "StateOverride not found: $overridePath"}
    $override=Get-Content $overridePath -Raw|ConvertFrom-Json
    foreach($property in $override.PSObject.Properties){Set-ObjectProperty $state $property.Name $property.Value}
}

$health=$null
if($state.visual_installed -and [int]$state.active_displays -ge [int]$state.expected_display_count){
    switch($HealthMode){
        'actual' {
            $health=Invoke-HealthCheck (Join-Path $runDir 'health-telemetry.csv')
            $state.post_install_health=$health.result
            $state.wgc_health=$health.result
        }
        'pass' {
            $health=[pscustomobject]@{exit_code=0;result='pass';telemetry=$null;simulated=$true}
            $state.post_install_health='pass';$state.wgc_health='pass'
        }
        'fail' {
            $health=[pscustomobject]@{exit_code=30;result='fail';telemetry=$null;simulated=$true}
            $state.post_install_health='fail';$state.wgc_health='fail'
        }
        'not-run' {}
    }
}

$scenario=[pscustomobject]@{
    id='visual_surrogate_live'
    description='Current two-monitor Visual system used as the surrogate Doxa software platform.'
    expected_outcome='auto'
    state=$state
}
$scenarioPath=Join-Path $runDir 'planner-input.json'
$scenario|ConvertTo-Json -Depth 10|Set-Content -Encoding UTF8 $scenarioPath
if($UseMuse){$plannerJson=& $simulate -Scenario $scenarioPath -Model $Model -UseMuse}
else{$plannerJson=& $simulate -Scenario $scenarioPath -Model $Model}
if($LASTEXITCODE-ne0){throw "simulate.ps1 failed: $LASTEXITCODE"}
$planner=$plannerJson|ConvertFrom-Json

$followUp=$null
if($UseMuse -and $planner.decision.outcome -eq 'ai_plan'){
    switch([string]$planner.decision.action){
        'run_minimal_capture_probe' {
            $followUp=Invoke-HealthCheck (Join-Path $runDir 'ai-followup-health-telemetry.csv')
        }
        'retry_post_install_health' {
            $followUp=Invoke-HealthCheck (Join-Path $runDir 'ai-followup-health-telemetry.csv')
        }
        'recheck_doxa_devices' {
            $followPath=Join-Path $runDir 'ai-followup-diagnostics.json'
            & $diagnosticsExe --out $followPath
            $followUp=[pscustomobject]@{exit_code=$LASTEXITCODE;result=if($LASTEXITCODE-eq0){'collected'}else{'failed'};diagnostics=$followPath}
        }
    }
}

$supportBundlePath=$null
if($planner.decision.action -eq 'collect_support_bundle'){
    $bundleDir=Join-Path $runDir 'support-bundle'
    if($health -and $health.telemetry -and (Test-Path $health.telemetry)){
        & $supportBundle -Configuration $Configuration -OutputDir $bundleDir -TelemetryPath $health.telemetry | Out-Null
    }else{
        & $supportBundle -Configuration $Configuration -OutputDir $bundleDir | Out-Null
    }
    $supportBundlePath=(Get-ChildItem $bundleDir -Filter '*.zip' -File|Sort-Object LastWriteTime -Descending|Select-Object -First 1 -ExpandProperty FullName)
}

$result=[pscustomobject]@{
    schema_version='1'
    run_dir=$runDir
    diagnostics=$diagnostics
    state=$state
    health=$health
    decision=$planner.decision
    follow_up=$followUp
    support_bundle=$supportBundlePath
}
$resultPath=Join-Path $runDir 'result.json'
$result|ConvertTo-Json -Depth 12|Set-Content -Encoding UTF8 $resultPath

$summary=[pscustomobject]@{
    run_dir=$runDir
    active_displays=$state.active_displays
    expected_displays=$state.expected_display_count
    health=$state.post_install_health
    outcome=$planner.decision.outcome
    action=$planner.decision.action
    used_ai=$planner.decision.used_ai
    follow_up=if($followUp){$followUp.result}else{$null}
    result_json=$resultPath
}
if($PassThru){$summary}else{$summary|Format-List}
