param(
    [string]$Version='0.1.0-alpha.2',
    [ValidateSet('Debug','Release','RelWithDebInfo','MinSizeRel')][string]$Configuration='Release',
    [switch]$SkipPackage
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest

$packagingRoot=$PSScriptRoot
$repoRoot=Resolve-Path (Join-Path $packagingRoot '..\..')
$outDir=Join-Path $packagingRoot 'out'
$artifactRoot=Join-Path $repoRoot 'artifacts'
New-Item -ItemType Directory -Force -Path $artifactRoot|Out-Null
$driverLog=Join-Path $artifactRoot "velopack_${Version}_lifecycle_verification.log"
$setupLog=Join-Path $artifactRoot "velopack_${Version}_setup.log"
Remove-Item $driverLog,$setupLog -Force -ErrorAction SilentlyContinue

function Log([string]$message){
    $line="$(Get-Date -Format o) $message"
    $line|Tee-Object -FilePath $driverLog -Append
}
function Wait-BoundedProcess([System.Diagnostics.Process]$process,[int]$timeoutMs,[string]$description){
    if($process.WaitForExit($timeoutMs)){return}
    try{Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue}catch{}
    throw "$description exceeded the $timeoutMs ms verification timeout."
}

try{
    Log "lifecycle_test_start version=$Version configuration=$Configuration"

    $installRoot=Join-Path $env:LOCALAPPDATA 'Standivarius.Visual'
    $installedExe=Join-Path $installRoot 'current\visual_app.exe'
    if(Test-Path $installedExe){
        $beforeVersion=(Get-Item $installedExe).VersionInfo.ProductVersion
        Log "installed_before=$beforeVersion path=$installedExe"
    }else{
        Log "installed_before=none expected_path=$installedExe"
    }

    if(-not$SkipPackage){
        $candidateScript=Join-Path $packagingRoot 'verify-alpha2-candidate.ps1'
        Log "candidate_verification_start script=$candidateScript"
        & $candidateScript -Version $Version -Configuration $Configuration 2>&1|ForEach-Object{Log ([string]$_)}
        Log 'candidate_verification_complete'
    }

    if(-not(Test-Path $outDir)){throw "Package output directory missing: $outDir"}
    $setup=Get-ChildItem $outDir -File -Filter '*-Setup.exe'|Sort-Object LastWriteTimeUtc -Descending|Select-Object -First 1
    if(-not$setup){throw "No Velopack Setup executable found in $outDir"}
    Log ("setup={0};bytes={1};utc={2}" -f $setup.FullName,$setup.Length,$setup.LastWriteTimeUtc.ToString('o'))

    # Setup --silent suppresses first-run application launch, which keeps this lifecycle
    # verification deterministic. Quote the log path explicitly because Windows PowerShell
    # 5.1 flattens Start-Process -ArgumentList arrays into one command line.
    $quotedSetupLog='"{0}"' -f $setupLog
    $setupArgs=@('--silent','--verbose','--log',$quotedSetupLog)
    Log "setup_start args=--silent --verbose --log $quotedSetupLog"
    $process=Start-Process -FilePath $setup.FullName -ArgumentList $setupArgs -PassThru
    Wait-BoundedProcess $process 180000 'Velopack Setup'
    Log "setup_exit_code=$($process.ExitCode)"
    if($process.ExitCode-ne0){throw "Velopack Setup failed with exit code $($process.ExitCode)"}
    if(-not(Test-Path $setupLog -PathType Leaf)){throw "Velopack setup log was not created: $setupLog"}

    if(-not(Test-Path $installedExe -PathType Leaf)){throw "Installed Visual executable missing after Setup: $installedExe"}
    $installedVersion=(Get-Item $installedExe).VersionInfo.ProductVersion
    Log "installed_after=$installedVersion"
    if(-not$installedVersion -or -not$installedVersion.StartsWith($Version)){
        throw "Installed Visual version '$installedVersion' does not match '$Version'"
    }

    $installedVelopack=Join-Path $installRoot 'current\velopack_libc.dll'
    $installedUpdate=Join-Path $installRoot 'Update.exe'
    if(-not(Test-Path $installedVelopack -PathType Leaf)){throw "Installed Velopack native runtime missing: $installedVelopack"}
    if(-not(Test-Path $installedUpdate -PathType Leaf)){throw "Velopack Update.exe missing: $installedUpdate"}
    Log ("installed_runtime_bytes={0}" -f (Get-Item $installedVelopack).Length)
    Log ("update_exe_bytes={0}" -f (Get-Item $installedUpdate).Length)

    $setupLogLines=@(Get-Content $setupLog -ErrorAction Stop)
    $hookFailureLines=@($setupLogLines|Where-Object{$_ -match '(?i)hook exited with non-zero exit code|install partially succeeded'})
    $failureLines=@($setupLogLines|Where-Object{$_ -match '(?i)\b(error|failed|failure|panic)\b'})
    $hookSuccessLines=@($setupLogLines|Where-Object{$_ -match '(?i)Hook executed successfully'})
    $installSuccessLines=@($setupLogLines|Where-Object{$_ -match '(?i)Installation completed successfully!'})
    Log "setup_log_failure_keyword_lines=$($failureLines.Count) hook_failure_lines=$($hookFailureLines.Count) hook_success_lines=$($hookSuccessLines.Count)"
    foreach($line in @($hookFailureLines+$failureLines)|Select-Object -Unique -First 20){Log "setup_log_flag=$line"}
    if($hookFailureLines.Count-gt0){throw 'Velopack lifecycle hook reported a non-zero exit or partial install.'}
    if($hookSuccessLines.Count-lt1){throw 'Velopack setup log did not report a successful application lifecycle hook.'}
    if($installSuccessLines.Count-lt1){throw 'Velopack setup log did not report successful installation completion.'}

    # Exercise the installed native UpdateManager without launching Visual's capture/UI path.
    # At the time alpha.2 is prepared the public feed contains only alpha.1, so a healthy
    # alpha.2 candidate should normally report no update. Exit 10 is also accepted so this
    # verification remains useful later when a newer prerelease exists.
    $updateLog=Join-Path $env:LOCALAPPDATA 'Standivarius\Visual\logs\visual_update.log'
    Remove-Item $updateLog -Force -ErrorAction SilentlyContinue
    Log 'installed_update_check_start'
    $checkProcess=Start-Process -FilePath $installedExe -ArgumentList @('--update-check') -PassThru
    Wait-BoundedProcess $checkProcess 90000 'Installed Visual update check'
    Log "installed_update_check_exit_code=$($checkProcess.ExitCode)"
    if($checkProcess.ExitCode-notin@(0,10)){
        throw "Installed Visual update check failed with exit code $($checkProcess.ExitCode)"
    }
    if(-not(Test-Path $updateLog -PathType Leaf)){throw "Installed update check did not create its log: $updateLog"}
    $updateLogLines=@(Get-Content $updateLog -ErrorAction Stop)
    foreach($line in $updateLogLines){Log "update_log=$line"}
    if(-not($updateLogLines|Where-Object{$_ -match 'installed_version='})){
        throw 'Installed update check log did not report the installed version.'
    }
    if(-not($updateLogLines|Where-Object{$_ -match 'result=(no_update|update_available)'})){
        throw 'Installed update check did not reach a healthy feed result.'
    }

    Log 'lifecycle_test_result=PASS'
    Write-Host "LIFECYCLE_RESULT=PASS"
    Write-Host "DRIVER_LOG=$driverLog"
    Write-Host "SETUP_LOG=$setupLog"
    Write-Host "UPDATE_LOG=$updateLog"
    Write-Host "INSTALLED_EXE=$installedExe"
}catch{
    Log "lifecycle_test_result=FAIL error=$($_.Exception.Message)"
    Write-Host "LIFECYCLE_RESULT=FAIL"
    Write-Host "DRIVER_LOG=$driverLog"
    Write-Host "SETUP_LOG=$setupLog"
    throw
}
