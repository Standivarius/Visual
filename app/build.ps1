param(
    [ValidateSet('Debug','Release','RelWithDebInfo','MinSizeRel')][string]$Configuration='Release',
    [string]$Version='',
    [switch]$OfflineDependencies
)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $MyInvocation.MyCommand.Path
$versionFile=Join-Path $root 'version.cmake'
if(-not$Version){
    $versionText=Get-Content -Raw $versionFile
    $versionMatch=[regex]::Match($versionText,'set\(VISUAL_DEFAULT_VERSION\s+"([^"]+)"\)')
    if(-not$versionMatch.Success){throw "Unable to read VISUAL_DEFAULT_VERSION from $versionFile"}
    $Version=$versionMatch.Groups[1].Value
}

$cmakeCommand=Get-Command cmake.exe -ErrorAction SilentlyContinue
if($cmakeCommand){
    $cmake=$cmakeCommand.Source
}else{
    $cmake='C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
}
if(-not(Test-Path $cmake)){throw "CMake not found: $cmake"}

# The native Velopack SDK is pinned independently from the .NET vpk packaging tool.
# It is downloaded once into an ignored repository-local cache and reused by later builds.
$dependencyLock=Get-Content -Raw (Join-Path $root 'packaging\release-dependencies.lock.json') | ConvertFrom-Json
$velopackVersion=[string](@($dependencyLock.dependencies | Where-Object id -eq 'velopack-native-sdk')[0].version)
$velopackRoot=Join-Path $root "third_party\velopack\$velopackVersion"
$velopackSdkScript=Join-Path $root 'packaging\get-velopack-sdk.ps1'
& $velopackSdkScript -Version $velopackVersion -DestinationRoot $velopackRoot -Offline:$OfflineDependencies | Out-Null
if(-not(Test-Path (Join-Path $velopackRoot 'include\Velopack.hpp'))){
    throw "Velopack SDK is incomplete: $velopackRoot"
}

Push-Location $root
try{
    $configureArgs=@('-S','.','-B','build','-G','Visual Studio 17 2022','-A','x64',"-DVELOPACK_ROOT=$velopackRoot")
    $configureArgs += "-DVISUAL_VERSION_SEMVER=$Version"
    & $cmake @configureArgs
    if($LASTEXITCODE-ne0){throw "CMake configure failed: $LASTEXITCODE"}

    & $cmake --build build --config $Configuration
    if($LASTEXITCODE-ne0){throw "CMake build failed: $LASTEXITCODE"}

    $ctest=Join-Path (Split-Path $cmake) 'ctest.exe'
    if(-not(Test-Path $ctest)){
        $ctestCommand=Get-Command ctest.exe -ErrorAction SilentlyContinue
        if(-not$ctestCommand){throw 'CTest not found'}
        $ctest=$ctestCommand.Source
    }
    & $ctest --test-dir build -C $Configuration --output-on-failure
    if($LASTEXITCODE-ne0){throw "CTest failed: $LASTEXITCODE"}
}finally{
    Pop-Location
}
