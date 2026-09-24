param(
    [string]$Version='0.1.0-alpha.2',
    [ValidateSet('Debug','Release','RelWithDebInfo','MinSizeRel')][string]$Configuration='Release'
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest

$packagingRoot=$PSScriptRoot
$appRoot=Split-Path -Parent $packagingRoot
$outDir=Join-Path $packagingRoot 'out'
$logPath=Join-Path $packagingRoot 'alpha2-candidate-verification.log'
Remove-Item $logPath -Force -ErrorAction SilentlyContinue

function Log([string]$message){
    $line="$(Get-Date -Format o) $message"
    $line|Tee-Object -FilePath $logPath -Append
}
function Get-PeMachine([string]$path){
    $stream=[IO.File]::OpenRead($path)
    try{
        $reader=New-Object IO.BinaryReader($stream)
        if($reader.ReadUInt16()-ne0x5A4D){throw "Not an MZ executable: $path"}
        $stream.Position=0x3C
        $peOffset=$reader.ReadInt32()
        $stream.Position=$peOffset
        if($reader.ReadUInt32()-ne0x00004550){throw "Invalid PE signature: $path"}
        return $reader.ReadUInt16()
    }finally{
        $stream.Dispose()
    }
}

try{
    Log "candidate_start version=$Version configuration=$Configuration"

    $packageScript=Join-Path $packagingRoot 'package.ps1'
    $packageText=Get-Content -Raw $packageScript
    if($packageText -match '(?i)--skipVeloAppCheck'){
        throw 'package.ps1 still contains --skipVeloAppCheck; lifecycle validation must remain enabled.'
    }

    & $packageScript -Version $Version -Configuration $Configuration -OutputDir $outDir 2>&1 | ForEach-Object { Log ([string]$_) }

    $buildDir=Join-Path $appRoot "build\$Configuration"
    $visualExe=Join-Path $buildDir 'visual_app.exe'
    $velopackDll=Join-Path $buildDir 'velopack_libc.dll'
    foreach($required in @($visualExe,$velopackDll)){
        if(-not(Test-Path $required -PathType Leaf)){throw "Required built file missing: $required"}
    }

    $productVersion=(Get-Item $visualExe).VersionInfo.ProductVersion
    Log "visual_product_version=$productVersion"
    if(-not $productVersion -or -not $productVersion.StartsWith($Version)){
        throw "visual_app.exe product version '$productVersion' does not match candidate '$Version'"
    }

    $exeMachine=Get-PeMachine $visualExe
    $dllMachine=Get-PeMachine $velopackDll
    Log ("visual_pe_machine=0x{0:X4}" -f $exeMachine)
    Log ("velopack_pe_machine=0x{0:X4}" -f $dllMachine)
    if($exeMachine-ne0x8664 -or $dllMachine-ne0x8664){
        throw "Expected x64 PE machine 0x8664; visual=0x$('{0:X4}' -f $exeMachine) velopack=0x$('{0:X4}' -f $dllMachine)"
    }

    $sdkZip=Join-Path $appRoot 'third_party\velopack\.downloads\velopack_libc_1.2.0.zip'
    if(Test-Path $sdkZip -PathType Leaf){
        $sdkHash=Get-FileHash -Algorithm SHA256 $sdkZip
        Log "velopack_sdk_zip_sha256=$($sdkHash.Hash.ToLowerInvariant())"
    }else{
        Log 'velopack_sdk_zip_sha256=unavailable_cached_sdk_without_zip'
    }

    $setup=@(Get-ChildItem $outDir -File -Filter '*-Setup.exe')
    $fullPackages=@(Get-ChildItem $outDir -File -Filter "*${Version}*-full.nupkg")
    $jsonFeeds=@(Get-ChildItem $outDir -File -Filter 'releases*.json')
    if($setup.Count-lt1){throw "No Velopack Setup executable was produced in $outDir"}
    if($fullPackages.Count-lt1){throw "No Velopack full package for $Version was produced in $outDir"}
    if($jsonFeeds.Count-lt1){throw "No Velopack JSON feed metadata was produced in $outDir"}
    $feedContainsVersion=$false
    foreach($feed in $jsonFeeds){
        if((Get-Content -Raw $feed.FullName) -match [regex]::Escape($Version)){$feedContainsVersion=$true;break}
    }
    if(-not$feedContainsVersion){throw "Velopack feed metadata does not contain candidate version $Version"}

    Log "setup_count=$($setup.Count) candidate_full_package_count=$($fullPackages.Count) json_feed_count=$($jsonFeeds.Count)"
    foreach($file in @($setup+$fullPackages+$jsonFeeds)){
        Log ("artifact={0};bytes={1}" -f $file.Name,$file.Length)
    }
    Log 'candidate_result=PASS'
    Write-Host "CANDIDATE_RESULT=PASS"
    Write-Host "CANDIDATE_LOG=$logPath"
}catch{
    Log "candidate_result=FAIL error=$($_.Exception.Message)"
    Write-Host "CANDIDATE_RESULT=FAIL"
    Write-Host "CANDIDATE_LOG=$logPath"
    throw
}
