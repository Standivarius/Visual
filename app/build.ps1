param(
    [ValidateSet('Debug','Release','RelWithDebInfo','MinSizeRel')][string]$Configuration='Release',
    [string]$Version=''
)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $MyInvocation.MyCommand.Path

$cmakeCommand=Get-Command cmake.exe -ErrorAction SilentlyContinue
if($cmakeCommand){
    $cmake=$cmakeCommand.Source
}else{
    $cmake='C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
}
if(-not(Test-Path $cmake)){throw "CMake not found: $cmake"}

Push-Location $root
try{
    $configureArgs=@('-S','.','-B','build','-G','Visual Studio 17 2022','-A','x64')
    if($Version){$configureArgs += "-DVISUAL_VERSION_SEMVER=$Version"}
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
