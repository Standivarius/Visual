param(
    [string]$Version='',
    [string]$PlannerUrl='',
    [string]$PlannerToken='',
    [int]$ExpectedDisplayCount=2,
    [string]$OutputDir='',
    [switch]$SkipBuild
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$appRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$repoRoot=Split-Path -Parent $appRoot
if(-not$Version){
    $text=Get-Content (Join-Path $appRoot 'version.cmake') -Raw
    $m=[regex]::Match($text,'VISUAL_DEFAULT_VERSION\s+"([^"]+)"')
    if(-not$m.Success){throw 'Unable to read Visual version.'}
    $Version=$m.Groups[1].Value
}
if([string]::IsNullOrWhiteSpace($PlannerUrl)){throw 'PlannerUrl is required for a portable cloud-assisted pilot kit.'}
if([string]::IsNullOrWhiteSpace($PlannerToken)){throw 'PlannerToken is required for a portable cloud-assisted pilot kit.'}
if(-not$OutputDir){$OutputDir=Join-Path $repoRoot "artifacts\doxa-pilot\$Version"}
$OutputDir=[IO.Path]::GetFullPath($OutputDir)
$work=Join-Path $OutputDir 'kit'
Remove-Item $OutputDir -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $work|Out-Null

$packageOut=Join-Path $OutputDir 'package'
$packageScript=Join-Path (Split-Path -Parent $PSScriptRoot) 'package.ps1'
if($SkipBuild){& $packageScript -Version $Version -Configuration Release -OutputDir $packageOut -SkipBuild}
else{& $packageScript -Version $Version -Configuration Release -OutputDir $packageOut}
if($LASTEXITCODE-ne0){throw "Visual packaging failed: $LASTEXITCODE"}
$setup=Get-ChildItem $packageOut -Filter '*Setup.exe' -File|Select-Object -First 1
if(-not$setup){throw 'Velopack setup executable not found.'}
Copy-Item $setup.FullName (Join-Path $work $setup.Name)
Copy-Item (Join-Path $PSScriptRoot 'run-pilot.ps1'),(Join-Path $PSScriptRoot 'reset-pilot.ps1') -Destination $work
Copy-Item (Join-Path (Split-Path -Parent $PSScriptRoot) 'doxa-setup\simulate.ps1'),(Join-Path (Split-Path -Parent $PSScriptRoot) 'doxa-setup\approved-actions.json') -Destination $work
$config=[ordered]@{
    schema_version='1'
    installer_file=$setup.Name
    visual_version=$Version
    expected_display_count=[Math]::Max(1,$ExpectedDisplayCount)
    planner_url=$PlannerUrl.TrimEnd('/')
    planner_token=$PlannerToken
}
$config|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 (Join-Path $work 'pilot-config.json')

$installCmd=@(
    '@echo off',
    'setlocal',
    'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0run-pilot.ps1" -ExercisePlanner',
    'set EXITCODE=%ERRORLEVEL%',
    'echo.',
    'if not "%EXITCODE%"=="0" echo Doxa pilot returned exit code %EXITCODE%.',
    'pause',
    'exit /b %EXITCODE%'
)
$installCmd|Set-Content -Encoding ASCII (Join-Path $work 'INSTALL_AND_TEST.cmd')
$resetCmd=@(
    '@echo off',
    'setlocal',
    'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0reset-pilot.ps1"',
    'set EXITCODE=%ERRORLEVEL%',
    'echo.',
    'pause',
    'exit /b %EXITCODE%'
)
$resetCmd|Set-Content -Encoding ASCII (Join-Path $work 'RESET_TEST_MACHINE.cmd')

$readme=@(
    "DOXA TWO-MONITOR PILOT $Version",
    '',
    '1. Keep two Windows monitors connected and extended.',
    '2. Double-click INSTALL_AND_TEST.cmd.',
    '3. The pilot silently installs Visual, runs the real graphics health check, then injects one safe ambiguous-failure scenario so the Doxa cloud / Dify / Muse path is exercised without damaging the PC.',
    '4. Review the final console result. Evidence is stored under %LOCALAPPDATA%\Doxa\Pilot.',
    '5. Double-click RESET_TEST_MACHINE.cmd when you want to return the PC to a clean Visual/Doxa test state.',
    '',
    'Reset deliberately leaves shared Microsoft runtimes, GPU drivers and vendor display drivers untouched.'
)
$readme|Set-Content -Encoding UTF8 (Join-Path $work 'README.txt')
$zip=Join-Path $OutputDir ("Doxa-Visual-TwoMonitor-Pilot-$Version.zip")
Compress-Archive -Path (Join-Path $work '*') -DestinationPath $zip -Force
Write-Host "Pilot kit: $zip"
Write-Host "Unpacked kit: $work"
