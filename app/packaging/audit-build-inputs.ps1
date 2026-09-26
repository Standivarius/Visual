param(
    [string]$BuildDir='',
    [string]$ReportPath='',
    [switch]$FailOnUnexpected
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest

$appRoot=Split-Path -Parent $PSScriptRoot
$repoRoot=Split-Path -Parent $appRoot
if(-not$BuildDir){$BuildDir=Join-Path $appRoot 'build'}
$BuildDir=[IO.Path]::GetFullPath($BuildDir)
if(-not(Test-Path $BuildDir -PathType Container)){throw "Build directory does not exist: $BuildDir"}
if(-not$ReportPath){$ReportPath=Join-Path $BuildDir 'observed-build-inputs.json'}
$ReportPath=[IO.Path]::GetFullPath($ReportPath)

function Normalize-Path([string]$path){
    $p=$path.Trim()
    if($p.StartsWith('\\?\')){$p=$p.Substring(4)}
    try{return [IO.Path]::GetFullPath($p)}catch{return $p}
}
function Under([string]$path,[string]$root){
    $p=$path.TrimEnd('\')+'\'
    $r=$root.TrimEnd('\')+'\'
    return $p.StartsWith($r,[StringComparison]::OrdinalIgnoreCase) -or $path.Equals($root,[StringComparison]::OrdinalIgnoreCase)
}

$vsRoot=$null
$vswhere='C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
if(Test-Path $vswhere){$vsRoot=(& $vswhere -latest -products '*' -property installationPath).Trim()}
$windowsSdkRoot='C:\Program Files (x86)\Windows Kits\10'
$windowsRoot=$env:WINDIR
$generatedRoot=$BuildDir
$pinnedNativeRoot=Join-Path $appRoot 'third_party\velopack'

$tracked=New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
& git -C $repoRoot ls-files | ForEach-Object {[void]$tracked.Add(($_ -replace '/','\'))}

$tlogs=@(Get-ChildItem $BuildDir -Recurse -File -Filter '*.read.*.tlog' -ErrorAction SilentlyContinue)
if($tlogs.Count-eq0){throw "No MSBuild *.read.*.tlog files found under $BuildDir"}
$inputs=New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
foreach($tlog in $tlogs){
    foreach($line in Get-Content $tlog.FullName){
        if([string]::IsNullOrWhiteSpace($line) -or $line.StartsWith('^')){continue}
        $candidate=Normalize-Path $line
        if([IO.Path]::IsPathRooted($candidate)){[void]$inputs.Add($candidate)}
    }
}

$records=@()
$unexpected=@()
foreach($path in ($inputs | Sort-Object)){
    $category=$null
    $relative=$null
    if(Under $path $repoRoot){
        $relative=$path.Substring($repoRoot.TrimEnd('\').Length).TrimStart('\')
        if(Under $path $generatedRoot){$category='generated_build'}
        elseif(Under $path $pinnedNativeRoot){$category='pinned_dependency_cache'}
        elseif($tracked.Contains($relative)){$category='tracked_repo'}
        else{$category='unexpected_repo_untracked'}
    } elseif($vsRoot -and (Under $path $vsRoot)){$category='visual_studio'}
    elseif(Under $path $windowsSdkRoot){$category='windows_sdk'}
    elseif($windowsRoot -and (Under $path $windowsRoot)){$category='windows'}
    else{$category='unexpected_external'}

    $recordData=[ordered]@{path=$path;category=$category}
    if($relative){$recordData.relative_path=$relative}
    $record=[pscustomobject]$recordData
    $records += $record
    if($category.StartsWith('unexpected_')){$unexpected += $record}
}

$counts=[ordered]@{}
$records | Group-Object category | Sort-Object Name | ForEach-Object {$counts[$_.Name]=$_.Count}
$report=[ordered]@{
    schema_version=1
    generated_utc=[DateTime]::UtcNow.ToString('o')
    source='MSBuild *.read.*.tlog observation'
    limitation='This audits observed compiler/link/resource/custom-build reads recorded by MSBuild. It is not a complete process-level file/network tracer and is not equivalent to mkcheck2/eBPF.'
    repo_root=$repoRoot
    build_dir=$BuildDir
    tlog_files=$tlogs.Count
    observed_inputs=$records.Count
    counts=$counts
    allowed_external_roots=@($vsRoot,$windowsSdkRoot,$windowsRoot)|Where-Object{$_}
    unexpected=$unexpected
    inputs=$records
}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $ReportPath)|Out-Null
$report|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 $ReportPath

[pscustomobject]@{tlogs=$tlogs.Count;observed_inputs=$records.Count;unexpected=$unexpected.Count;report=$ReportPath}
if($unexpected.Count-gt0){
    $unexpected|ForEach-Object{Write-Warning ("Unexpected build input [{0}]: {1}" -f $_.category,$_.path)}
    if($FailOnUnexpected){throw "Observed $($unexpected.Count) unexpected build input(s). See $ReportPath"}
}
