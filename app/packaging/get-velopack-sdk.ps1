param(
    [string]$Version='1.2.0',
    [string]$DestinationRoot=''
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest

$appRoot=Split-Path -Parent $PSScriptRoot
if(-not$DestinationRoot){$DestinationRoot=Join-Path $appRoot "third_party\velopack\$Version"}
$DestinationRoot=[IO.Path]::GetFullPath($DestinationRoot)

$assetName="velopack_libc_${Version}.zip"
$assetUrl="https://github.com/velopack/velopack/releases/download/$Version/$assetName"
$expectedSha256ByVersion=@{
    '1.2.0'='547262ed7a1ab1ff62f580aa53851ede2f1a451ac61b8974eb7bc01117488835'
}
if(-not$expectedSha256ByVersion.ContainsKey($Version)){throw "No pinned SHA-256 is registered for Velopack SDK $Version"}
$expectedSha256=$expectedSha256ByVersion[$Version]

$cacheRoot=Split-Path -Parent $DestinationRoot
$downloadRoot=Join-Path $cacheRoot '.downloads'
$zipPath=Join-Path $downloadRoot $assetName
$tempExtract=Join-Path $cacheRoot (".extract-{0}-{1}" -f $Version,[Guid]::NewGuid().ToString('N'))

$requiredRelative=@(
    'include\Velopack.h',
    'include\Velopack.hpp',
    'lib\velopack_libc_win_x64_msvc.dll',
    'lib\velopack_libc_win_x64_msvc.dll.lib'
)

function Test-SdkLayout([string]$root){
    if(-not(Test-Path $root -PathType Container)){return $false}
    foreach($relative in $requiredRelative){
        $candidate=Join-Path $root $relative
        if(-not(Test-Path $candidate -PathType Leaf)){return $false}
        if((Get-Item $candidate).Length-le0){return $false}
    }
    return $true
}

function Assert-ZipHash([string]$path){
    $actual=(Get-FileHash -Algorithm SHA256 $path).Hash.ToLowerInvariant()
    if($actual-ne$expectedSha256){
        Remove-Item $path -Force -ErrorAction SilentlyContinue
        throw "Velopack SDK ZIP SHA-256 mismatch. Expected $expectedSha256, got $actual"
    }
}

if((Test-SdkLayout $DestinationRoot) -and (Test-Path $zipPath -PathType Leaf)){
    Assert-ZipHash $zipPath
    Write-Host "Velopack C/C++ SDK $Version already available at $DestinationRoot"
    Write-Output $DestinationRoot
    return
}

New-Item -ItemType Directory -Force -Path $cacheRoot,$downloadRoot|Out-Null
Remove-Item -Recurse -Force $tempExtract -ErrorAction SilentlyContinue

try{
    if(-not(Test-Path $zipPath -PathType Leaf)){
        Write-Host "Downloading pinned Velopack C/C++ SDK $Version"
        Write-Host $assetUrl
        $oldProtocol=[Net.ServicePointManager]::SecurityProtocol
        try{
            [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
            Invoke-WebRequest -Uri $assetUrl -OutFile $zipPath -UseBasicParsing
        }finally{
            [Net.ServicePointManager]::SecurityProtocol=$oldProtocol
        }
    }
    Assert-ZipHash $zipPath

    New-Item -ItemType Directory -Force -Path $tempExtract|Out-Null
    Expand-Archive -Path $zipPath -DestinationPath $tempExtract -Force

    $candidateRoot=$tempExtract
    if(-not(Test-SdkLayout $candidateRoot)){
        $children=@(Get-ChildItem $tempExtract -Directory)
        if($children.Count-eq1 -and (Test-SdkLayout $children[0].FullName)){
            $candidateRoot=$children[0].FullName
        }
    }
    if(-not(Test-SdkLayout $candidateRoot)){
        $found=@(Get-ChildItem $tempExtract -Recurse -File|ForEach-Object{$_.FullName.Substring($tempExtract.Length).TrimStart('\')}) -join '; '
        throw "Downloaded Velopack SDK layout is not the expected C/C++ layout for $Version. Files: $found"
    }

    Remove-Item -Recurse -Force $DestinationRoot -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $DestinationRoot|Out-Null
    Copy-Item -Path (Join-Path $candidateRoot '*') -Destination $DestinationRoot -Recurse -Force
    if(-not(Test-SdkLayout $DestinationRoot)){throw "Velopack SDK validation failed after extraction: $DestinationRoot"}

    @(
        "version=$Version",
        "source=$assetUrl",
        "sha256=$expectedSha256",
        "acquired_utc=$([DateTime]::UtcNow.ToString('o'))"
    ) | Set-Content -Encoding UTF8 (Join-Path $DestinationRoot 'visual-sdk-source.txt')

    Write-Host "Velopack C/C++ SDK $Version ready at $DestinationRoot"
    Write-Output $DestinationRoot
}finally{
    Remove-Item -Recurse -Force $tempExtract -ErrorAction SilentlyContinue
}
