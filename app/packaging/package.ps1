param(
    [string]$Version='',
    [ValidateSet('Debug','Release','RelWithDebInfo','MinSizeRel')][string]$Configuration='Release',
    [string]$OutputDir='',
    [switch]$SkipBuild,
    [switch]$EnterpriseMsi
)
$ErrorActionPreference='Stop'

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

if(-not $SkipBuild){
    & (Join-Path $appRoot 'build.ps1') -Configuration $Configuration -Version $Version
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
Use visual_diagnostics.exe, or the support-bundle script from the source repository, when reporting a problem.
"@ | Set-Content -Encoding UTF8 (Join-Path $stage 'README.txt')

@"
visual_version=$Version
app_id=Standivarius.Visual
channel=alpha
velopack_native_sdk=1.2.0
built_utc=$([DateTime]::UtcNow.ToString('o'))
"@ | Set-Content -Encoding UTF8 (Join-Path $stage 'release-info.txt')

$dotnetSdkVersion='8.0.425'
$dotnetCommand=Get-Command dotnet.exe -ErrorAction SilentlyContinue
$dotnetPath=if($dotnetCommand){$dotnetCommand.Source}else{$null}
$sdks=if($dotnetPath){@(& $dotnetPath --list-sdks)}else{@()}
$hasPinnedSdk=@($sdks|Where-Object{$_ -match ('^'+[regex]::Escape($dotnetSdkVersion)+'\s')}).Count-gt0
if(-not$hasPinnedSdk){
    $dotnetSdkScript=Join-Path $PSScriptRoot 'get-dotnet-sdk.ps1'
    $dotnetPath=& $dotnetSdkScript -Version $dotnetSdkVersion
    $sdks=@(& $dotnetPath --list-sdks)
    $hasPinnedSdk=@($sdks|Where-Object{$_ -match ('^'+[regex]::Escape($dotnetSdkVersion)+'\s')}).Count-gt0
}
if(-not$dotnetPath -or -not$hasPinnedSdk){throw "Unable to provision pinned .NET SDK $dotnetSdkVersion for the Velopack CLI."}
$env:DOTNET_CLI_TELEMETRY_OPTOUT='1'
$env:DOTNET_SKIP_FIRST_TIME_EXPERIENCE='1'
$env:DOTNET_NOLOGO='1'

Push-Location $repoRoot
try{
    & $dotnetPath tool restore
    if($LASTEXITCODE-ne0){throw "dotnet tool restore failed: $LASTEXITCODE"}

    # Do not bypass VelopackApp validation. visual_app.exe is linked against the
    # pinned native SDK and runs VelopackApp at the start of the real wWinMain.
    # --yes makes repeated local/CI packaging deterministic when generic Setup/feed
    # filenames already exist in the retained release directory.
    $packArgs=@(
        '--yes','pack',
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
        # Enterprise deployments need a machine-wide artifact that can be deployed
        # by Intune/ConfigMgr under SYSTEM without relying on the end user's rights.
        # Velopack generates the MSI via WiX and keeps the same app/update layout.
        $packArgs += @('--msi','true','--instLocation','PerMachine')
    }
    & $dotnetPath tool run vpk @packArgs
    if($LASTEXITCODE-ne0){throw "Velopack packaging failed: $LASTEXITCODE"}
}finally{
    Pop-Location
}

Write-Host "Visual $Version package created in $OutputDir"
Get-ChildItem $OutputDir | Select-Object Name,Length,LastWriteTime
