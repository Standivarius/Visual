param(
    [string]$Version='8.0.425',
    [string]$DestinationRoot='',
    [switch]$Offline
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest

$appRoot=Split-Path -Parent $PSScriptRoot
$repoRoot=Split-Path -Parent $appRoot
if(-not$DestinationRoot){$DestinationRoot=Join-Path $repoRoot "tools\dotnet\$Version"}
$DestinationRoot=[IO.Path]::GetFullPath($DestinationRoot)
$lockPath=Join-Path $PSScriptRoot 'release-dependencies.lock.json'
$lock=Get-Content -Raw $lockPath | ConvertFrom-Json
$dependency=@($lock.dependencies | Where-Object {$_.id-eq'dotnet-sdk' -and $_.version-eq$Version})
if($dependency.Count-ne1){throw "No locked .NET SDK dependency is registered for version $Version"}
$dependency=$dependency[0]
$zipUrl=[string]$dependency.url
if($dependency.hash.algorithm-ne'SHA512'){throw ".NET SDK lock must use SHA512"}
$expectedSha512=([string]$dependency.hash.value).ToLowerInvariant()
$downloadRoot=Join-Path $repoRoot 'tools\dotnet\.downloads'
$zipPath=Join-Path $downloadRoot "dotnet-sdk-$Version-win-x64.zip"
$dotnetExe=Join-Path $DestinationRoot 'dotnet.exe'
$marker=Join-Path $DestinationRoot 'visual-sdk-source.txt'

function Test-Sdk([string]$root){
    $exe=Join-Path $root 'dotnet.exe'
    $sdkDir=Join-Path $root "sdk\$Version"
    $sourceMarker=Join-Path $root 'visual-sdk-source.txt'
    if(-not((Test-Path $exe -PathType Leaf) -and (Test-Path $sdkDir -PathType Container) -and (Test-Path $sourceMarker -PathType Leaf))){return $false}
    $markerText=Get-Content -Raw $sourceMarker
    return $markerText.Contains("version=$Version") -and $markerText.Contains("sha512=$expectedSha512")
}

if(Test-Sdk $DestinationRoot){
    Write-Output $dotnetExe
    return
}

New-Item -ItemType Directory -Force -Path $downloadRoot|Out-Null
if(-not(Test-Path $zipPath -PathType Leaf)){
    if($Offline){throw "Offline dependency mode: .NET SDK archive is missing: $zipPath"}
    Invoke-WebRequest -UseBasicParsing -Uri $zipUrl -OutFile $zipPath
}
$actual=(Get-FileHash -Algorithm SHA512 $zipPath).Hash.ToLowerInvariant()
if($actual-ne$expectedSha512){
    Remove-Item $zipPath -Force -ErrorAction SilentlyContinue
    throw "Pinned .NET SDK ZIP SHA-512 mismatch. Expected $expectedSha512, got $actual"
}
Remove-Item -Recurse -Force $DestinationRoot -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $DestinationRoot|Out-Null
Expand-Archive -Path $zipPath -DestinationPath $DestinationRoot -Force
@"
version=$Version
source=$zipUrl
sha512=$expectedSha512
"@ | Set-Content -Encoding UTF8 $marker
if(-not(Test-Sdk $DestinationRoot)){throw "Pinned .NET SDK $Version extraction validation failed: $DestinationRoot"}
Write-Output $dotnetExe
