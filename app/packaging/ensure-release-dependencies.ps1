param(
    [switch]$Offline,
    [string]$RepoRoot=''
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest

if(-not$RepoRoot){$RepoRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)}
$RepoRoot=[IO.Path]::GetFullPath($RepoRoot)
$lockPath=Join-Path $PSScriptRoot 'release-dependencies.lock.json'
$lock=Get-Content -Raw $lockPath | ConvertFrom-Json

function Get-Dependency([string]$id){
    $d=@($lock.dependencies | Where-Object id -eq $id)
    if($d.Count-ne1){throw "Expected exactly one dependency '$id' in $lockPath"}
    return $d[0]
}
function Assert-Hash([string]$path,$hash){
    if(-not(Test-Path $path -PathType Leaf)){throw "Pinned dependency cache file is missing: $path"}
    $actual=(Get-FileHash -Algorithm $hash.algorithm $path).Hash.ToLowerInvariant()
    $expected=([string]$hash.value).ToLowerInvariant()
    if($actual-ne$expected){throw "Pinned dependency hash mismatch for $path. Expected $expected, got $actual"}
}
function Ensure-Download($dependency){
    $path=Join-Path $RepoRoot ([string]$dependency.cache_path -replace '/','\')
    if(-not(Test-Path $path -PathType Leaf)){
        if($Offline){throw "Offline dependency mode: required cache file is missing: $path"}
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $path)|Out-Null
        Write-Host "Downloading pinned dependency $($dependency.id) $($dependency.version)"
        Invoke-WebRequest -UseBasicParsing -Uri $dependency.url -OutFile $path
    }
    Assert-Hash $path $dependency.hash
    return $path
}

$native=Get-Dependency 'velopack-native-sdk'
$dotnet=Get-Dependency 'dotnet-sdk'
$vpk=Get-Dependency 'velopack-cli'
$toolManifestPath=Join-Path $RepoRoot '.config\dotnet-tools.json'
$toolManifest=Get-Content -Raw $toolManifestPath | ConvertFrom-Json
if([string]$toolManifest.tools.vpk.version-ne[string]$vpk.version){throw "dotnet tool manifest vpk version $($toolManifest.tools.vpk.version) does not match locked version $($vpk.version)"}
$nativeZip=Ensure-Download $native
$dotnetZip=Ensure-Download $dotnet
$vpkPackage=Ensure-Download $vpk

$nugetDir=Split-Path -Parent $vpkPackage
$nugetConfig=Join-Path $RepoRoot 'tools\release-cache\NuGet.offline.config'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $nugetConfig)|Out-Null
$escaped=[Security.SecurityElement]::Escape($nugetDir)
@"
<?xml version="1.0" encoding="utf-8"?>
<configuration>
  <packageSources>
    <clear />
    <add key="visual-release-cache" value="$escaped" />
  </packageSources>
</configuration>
"@ | Set-Content -Encoding UTF8 $nugetConfig

$summary=[ordered]@{
    lock_file=$lockPath
    lock_sha256=(Get-FileHash -Algorithm SHA256 $lockPath).Hash.ToLowerInvariant()
    offline=[bool]$Offline
    velopack_native_zip=$nativeZip
    dotnet_sdk_zip=$dotnetZip
    velopack_cli_nupkg=$vpkPackage
    nuget_config=$nugetConfig
}
[pscustomobject]$summary
