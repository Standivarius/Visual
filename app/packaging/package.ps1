param(
    [string]$Version='',
    [ValidateSet('Debug','Release','RelWithDebInfo','MinSizeRel')][string]$Configuration='Release',
    [string]$OutputDir='',
    [switch]$SkipBuild
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
foreach($required in @($visualExe,$diagnosticsExe)){
    if(-not(Test-Path $required)){throw "Required package file not found: $required"}
}

Remove-Item -Recurse -Force $stage -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $stage,$OutputDir|Out-Null
Copy-Item $visualExe,$diagnosticsExe -Destination $stage

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
built_utc=$([DateTime]::UtcNow.ToString('o'))
"@ | Set-Content -Encoding UTF8 (Join-Path $stage 'release-info.txt')

$dotnet=Get-Command dotnet.exe -ErrorAction SilentlyContinue
if(-not $dotnet){throw 'The .NET SDK is required to run the pinned Velopack vpk tool.'}
$sdks=& $dotnet.Source --list-sdks
if(-not $sdks){throw 'dotnet is installed but no .NET SDK is available. Install an SDK or run packaging in GitHub Actions.'}

Push-Location $repoRoot
try{
    & $dotnet.Source tool restore
    if($LASTEXITCODE-ne0){throw "dotnet tool restore failed: $LASTEXITCODE"}

    & $dotnet.Source tool run vpk pack `
        --packId 'Standivarius.Visual' `
        --packVersion $Version `
        --packDir $stage `
        --mainExe 'visual_app.exe' `
        --packAuthors 'Standivarius' `
        --packTitle 'Visual Alpha' `
        --channel 'alpha' `
        --framework 'vcredist143-x64' `
        --skipVeloAppCheck true `
        --releaseNotes (Join-Path $PSScriptRoot 'RELEASE_NOTES.md') `
        --outputDir $OutputDir
    if($LASTEXITCODE-ne0){throw "Velopack packaging failed: $LASTEXITCODE"}
}finally{
    Pop-Location
}

Write-Host "Visual $Version package created in $OutputDir"
Get-ChildItem $OutputDir | Select-Object Name,Length,LastWriteTime
