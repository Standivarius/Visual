param(
    [string]$Version='8.0.425',
    [string]$DestinationRoot=''
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest

$appRoot=Split-Path -Parent $PSScriptRoot
$repoRoot=Split-Path -Parent $appRoot
if(-not$DestinationRoot){$DestinationRoot=Join-Path $repoRoot "tools\dotnet\$Version"}
$DestinationRoot=[IO.Path]::GetFullPath($DestinationRoot)
$zipUrl="https://builds.dotnet.microsoft.com/dotnet/Sdk/$Version/dotnet-sdk-$Version-win-x64.zip"
$expectedSha512ByVersion=@{
    '8.0.425'='f0b6f15bf6f1a0507205c0cb102ab99e1dee875c4682c8ed94665be1d580186a06b21455e83b3a01a0ff7f4cd887b67420f2e2fe09ed985534a4cea488ae1af9'
}
if(-not$expectedSha512ByVersion.ContainsKey($Version)){throw "No pinned SHA-512 is registered for .NET SDK $Version"}
$expectedSha512=$expectedSha512ByVersion[$Version]
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
