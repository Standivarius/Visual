param(
    [string]$Version='',
    [ValidateSet('Debug','Release','RelWithDebInfo','MinSizeRel')][string]$Configuration='Release',
    [string]$OutputDir='',
    [switch]$SkipBuild,
    [switch]$EnterpriseMsi,
    [switch]$OfflineDependencies
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest

$appRoot=Split-Path -Parent $PSScriptRoot
$repoRoot=Split-Path -Parent $appRoot
$versionFile=Join-Path $appRoot 'version.cmake'

if(-not $Version){
    $versionText=Get-Content -Raw $versionFile
    $match=[regex]::Match($versionText,'set\(VISUAL_DEFAULT_VERSION\s+"([^"]+)"\)')
    if(-not $match.Success){throw "Unable to read VISUAL_DEFAULT_VERSION from $versionFile"}
    $Version=$match.Groups[1].Value
}
if($Version -notmatch '^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?$'){
    throw "Version is not valid SemVer for Visual packaging: $Version"
}
if(-not $OutputDir){$OutputDir=Join-Path $PSScriptRoot 'out'}
$OutputDir=[IO.Path]::GetFullPath($OutputDir)
$stage=Join-Path $PSScriptRoot 'stage'

# All downloaded build/package inputs are content-pinned by one tracked lock file.
# In OfflineDependencies mode this step is verification-only and fails if any
# required cache file is absent or has the wrong digest.
$ensureDependencies=Join-Path $PSScriptRoot 'ensure-release-dependencies.ps1'
$dependencyState=& $ensureDependencies -Offline:$OfflineDependencies -RepoRoot $repoRoot
if(-not$dependencyState){throw 'Release dependency verification returned no state.'}
$lock=Get-Content -Raw (Join-Path $PSScriptRoot 'release-dependencies.lock.json') | ConvertFrom-Json
$nativeSdk=@($lock.dependencies|Where-Object id -eq 'velopack-native-sdk')[0]

if(-not $SkipBuild){
    & (Join-Path $appRoot 'build.ps1') -Configuration $Configuration -Version $Version -OfflineDependencies:$OfflineDependencies
    if($LASTEXITCODE-ne0){throw "Visual build failed: $LASTEXITCODE"}
}

$buildDir=Join-Path $appRoot "build\$Configuration"
$visualExe=Join-Path $buildDir 'visual_app.exe'
$diagnosticsExe=Join-Path $buildDir 'visual_diagnostics.exe'
$velopackRuntime=Join-Path $buildDir 'velopack_libc.dll'
foreach($required in @($visualExe,$diagnosticsExe,$velopackRuntime)){
    if(-not(Test-Path $required)){throw "Required package file not found: $required"}
}

Remove-Item -Recurse -Force $stage -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $stage,$OutputDir|Out-Null
$buildInputReport=Join-Path $OutputDir ("Standivarius.Visual-$Version.build-inputs.json")
& (Join-Path $PSScriptRoot 'audit-build-inputs.ps1') -BuildDir (Split-Path -Parent $buildDir) -ReportPath $buildInputReport -FailOnUnexpected | Out-Host
Copy-Item $visualExe,$diagnosticsExe,$velopackRuntime -Destination $stage

@"
Visual Alpha $Version
Package: Standivarius.Visual
Channel: alpha

Global hotkeys:
  Ctrl+Alt+1  1x
  Ctrl+Alt+2  2x
  Ctrl+Alt+4  4x
  Ctrl+Alt+0  normal view / return
  Ctrl+Alt+T  tracking on/off
  Ctrl+Alt+Q  exit Visual

This is an unsigned development alpha. Windows may show SmartScreen or publisher warnings.
On the first installed launch, Visual automatically checks the two-monitor graphics path and connects to Doxa cloud setup support.
Use visual_diagnostics.exe, or the support-bundle script from the source repository, when reporting a problem.
"@ | Set-Content -Encoding UTF8 (Join-Path $stage 'README.txt')

@"
visual_version=$Version
app_id=Standivarius.Visual
channel=alpha
velopack_native_sdk=$($nativeSdk.version)
built_utc=$([DateTime]::UtcNow.ToString('o'))
"@ | Set-Content -Encoding UTF8 (Join-Path $stage 'release-info.txt')

$dotnetSdk=@($lock.dependencies|Where-Object id -eq 'dotnet-sdk')[0]
$dotnetSdkScript=Join-Path $PSScriptRoot 'get-dotnet-sdk.ps1'
$dotnetPath=& $dotnetSdkScript -Version $dotnetSdk.version -Offline:$OfflineDependencies
if(-not$dotnetPath -or -not(Test-Path $dotnetPath)){throw "Unable to provision pinned .NET SDK $($dotnetSdk.version) for the Velopack CLI."}
if((& $dotnetPath --version).Trim()-ne$dotnetSdk.version){throw "Pinned dotnet executable did not report expected SDK $($dotnetSdk.version)."}

$releaseCache=Join-Path $repoRoot 'tools\release-cache'
$env:DOTNET_CLI_HOME=Join-Path $releaseCache 'dotnet-home'
$env:NUGET_PACKAGES=Join-Path $releaseCache 'packages'
$env:DOTNET_CLI_TELEMETRY_OPTOUT='1'
$env:DOTNET_SKIP_FIRST_TIME_EXPERIENCE='1'
$env:DOTNET_NOLOGO='1'
New-Item -ItemType Directory -Force -Path $env:DOTNET_CLI_HOME,$env:NUGET_PACKAGES|Out-Null

Push-Location $repoRoot
try{
    # Restore the manifest tool from the verified local NuGet source only. The
    # generated NuGet config clears all external package sources.
    & $dotnetPath tool restore --configfile $dependencyState.nuget_config
    if($LASTEXITCODE-ne0){throw "offline dotnet tool restore failed: $LASTEXITCODE"}

    # Do not bypass VelopackApp validation. visual_app.exe is linked against the
    # pinned native SDK and runs VelopackApp at the start of the real wWinMain.
    $packArgs=@(
        '--yes','--skip-updates','true','pack',
        '--packId','Standivarius.Visual',
        '--packVersion',$Version,
        '--packDir',$stage,
        '--mainExe','visual_app.exe',
        '--packAuthors','Standivarius',
        '--packTitle','Visual Alpha',
        '--channel','alpha',
        '--framework','vcredist143-x64',
        '--releaseNotes',(Join-Path $PSScriptRoot 'RELEASE_NOTES.md'),
        '--outputDir',$OutputDir
    )
    if($EnterpriseMsi){
        $packArgs += @('--msi','true','--instLocation','PerMachine')
    }
    & $dotnetPath tool run vpk @packArgs
    if($LASTEXITCODE-ne0){throw "Velopack packaging failed: $LASTEXITCODE"}
}finally{
    Pop-Location
}

$evidenceScript=Join-Path $PSScriptRoot 'generate-release-evidence.ps1'
& $evidenceScript -Version $Version -OutputDir $OutputDir -Configuration $Configuration | Out-Host

Write-Host "Visual $Version package created in $OutputDir"
Get-ChildItem $OutputDir -File | Select-Object Name,Length,LastWriteTime
