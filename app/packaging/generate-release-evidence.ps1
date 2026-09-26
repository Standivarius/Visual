param(
    [Parameter(Mandatory=$true)][string]$Version,
    [Parameter(Mandatory=$true)][string]$OutputDir,
    [string]$Configuration='Release'
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest

$appRoot=Split-Path -Parent $PSScriptRoot
$repoRoot=Split-Path -Parent $appRoot
$OutputDir=[IO.Path]::GetFullPath($OutputDir)
$lockPath=Join-Path $PSScriptRoot 'release-dependencies.lock.json'
$lock=Get-Content -Raw $lockPath | ConvertFrom-Json

function File-Record([string]$path){
    if(-not(Test-Path $path -PathType Leaf)){return $null}
    $i=Get-Item $path
    return [ordered]@{name=$i.Name;path=$i.FullName;bytes=$i.Length;sha256=(Get-FileHash -Algorithm SHA256 $i.FullName).Hash.ToLowerInvariant()}
}
function Command-Version([string]$command,[string[]]$args){
    try {
        $c=Get-Command $command -ErrorAction Stop
        $text=@(& $c.Source @args 2>&1) -join "`n"
        return [ordered]@{path=$c.Source;version_text=$text.Trim()}
    } catch { return $null }
}

$commit=(& git -C $repoRoot rev-parse HEAD).Trim()
$trackedDirty=@(& git -C $repoRoot status --porcelain --untracked-files=no)
$cmake=Command-Version 'cmake.exe' @('--version')
if(-not$cmake){
    $fallback='C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    if(Test-Path $fallback){$text=@(& $fallback --version 2>&1)-join"`n";$cmake=[ordered]@{path=$fallback;version_text=$text.Trim()}}
}
$vswhere='C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$vs=$null
if(Test-Path $vswhere){
    $raw=& $vswhere -latest -products '*' -format json | ConvertFrom-Json
    if($raw){$vs=[ordered]@{installationVersion=$raw[0].installationVersion;displayVersion=$raw[0].catalog.productDisplayVersion;installationPath=$raw[0].installationPath}}
}
$sdkRoot='C:\Program Files (x86)\Windows Kits\10\Include'
$windowsSdk=if(Test-Path $sdkRoot){@(Get-ChildItem $sdkRoot -Directory | Select-Object -ExpandProperty Name | Sort-Object {[version]$_} | Select-Object -Last 1)[0]}else{$null}
$dotnetExe=Join-Path $repoRoot ("tools\dotnet\{0}\dotnet.exe" -f (@($lock.dependencies|Where-Object id -eq 'dotnet-sdk')[0].version))
$dotnetVersion=if(Test-Path $dotnetExe){(& $dotnetExe --version).Trim()}else{$null}
$cloudTokenEmbedded=$false
$cloudConfig=Join-Path $appRoot 'build\generated\visual_cloud_config.h'
if(Test-Path $cloudConfig){
    $cloudText=Get-Content -Raw $cloudConfig
    $cloudMatch=[regex]::Match($cloudText,'kClientToken\[\]\s*=\s*L"([^"]*)"')
    if($cloudMatch.Success){$cloudTokenEmbedded=$cloudMatch.Groups[1].Value.Length-gt0}
}

$artifacts=@()
Get-ChildItem $OutputDir -File -ErrorAction SilentlyContinue | ForEach-Object {$artifacts += (File-Record $_.FullName)}
$buildDir=Join-Path $appRoot "build\$Configuration"
$runtime=@()
foreach($name in @('visual_app.exe','visual_diagnostics.exe','velopack_libc.dll')){$r=File-Record (Join-Path $buildDir $name);if($r){$runtime+=$r}}

$provenance=[ordered]@{
    schema_version=1
    subject='Standivarius.Visual'
    version=$Version
    generated_utc=[DateTime]::UtcNow.ToString('o')
    source=[ordered]@{git_commit=$commit;tracked_worktree_dirty=($trackedDirty.Count-gt0)}
    reproducibility=[ordered]@{level=$lock.reproducibility_level;dependency_lock_sha256=(Get-FileHash -Algorithm SHA256 $lockPath).Hash.ToLowerInvariant();note='Downloaded release dependencies are content-pinned. MSVC, CMake and Windows SDK remain host toolchain inputs and are recorded rather than hermetically pinned.'}
    secret_build_inputs=[ordered]@{DOXA_CLOUD_CLIENT_TOKEN_embedded_present=$cloudTokenEmbedded;value_recorded=$false}
    host_toolchain=[ordered]@{os=[Environment]::OSVersion.VersionString;architecture=$env:PROCESSOR_ARCHITECTURE;visual_studio=$vs;cmake=$cmake;windows_sdk=$windowsSdk;dotnet_sdk=$dotnetVersion}
    locked_dependencies=$lock.dependencies
    runtime_files=$runtime
    release_artifacts=$artifacts
}
$provPath=Join-Path $OutputDir ("Standivarius.Visual-$Version.provenance.json")
$provenance|ConvertTo-Json -Depth 12 | Set-Content -Encoding UTF8 $provPath

$components=@()
$native=@($lock.dependencies|Where-Object id -eq 'velopack-native-sdk')[0]
$components += [ordered]@{
    type='library';name='Velopack native runtime';version=$native.version
    purl="pkg:github/velopack/velopack@$($native.version)"
    hashes=@([ordered]@{alg='SHA-256';content=$native.hash.value})
    properties=@([ordered]@{name='visual:role';value='runtime-and-packaging-library'})
}
$tools=@()
foreach($d in $lock.dependencies | Where-Object {$_.id -in @('dotnet-sdk','velopack-cli')}){
    $tools += [ordered]@{type='application';name=$d.id;version=$d.version;hashes=@([ordered]@{alg=if($d.hash.algorithm-eq'SHA512'){'SHA-512'}else{'SHA-256'};content=$d.hash.value})}
}
$sbom=[ordered]@{
    bomFormat='CycloneDX';specVersion='1.6';serialNumber=('urn:uuid:'+([guid]::NewGuid().ToString()));version=1
    metadata=[ordered]@{
        timestamp=[DateTime]::UtcNow.ToString('o')
        component=[ordered]@{type='application';name='Standivarius Visual';version=$Version;'bom-ref'="pkg:generic/Standivarius.Visual@$Version"}
        tools=[ordered]@{components=$tools}
        properties=@(
            [ordered]@{name='visual:git-commit';value=$commit},
            [ordered]@{name='visual:dependency-lock-sha256';value=(Get-FileHash -Algorithm SHA256 $lockPath).Hash.ToLowerInvariant()},
            [ordered]@{name='visual:reproducibility-level';value=$lock.reproducibility_level}
        )
    }
    components=$components
}
$sbomPath=Join-Path $OutputDir ("Standivarius.Visual-$Version.sbom.cdx.json")
$sbom|ConvertTo-Json -Depth 12 | Set-Content -Encoding UTF8 $sbomPath

[pscustomobject]@{provenance=$provPath;sbom=$sbomPath}
