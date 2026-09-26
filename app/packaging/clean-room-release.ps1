param(
    [Parameter(Mandatory=$true)][string]$Version,
    [string]$OutputDir='',
    [string]$Commit='HEAD',
    [switch]$KeepWorktree,
    [switch]$RequireCloudToken
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest

$repoRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$repoRoot=[IO.Path]::GetFullPath($repoRoot)
if(-not$OutputDir){$OutputDir=Join-Path $repoRoot "artifacts\clean-room-release-$Version"}
$OutputDir=[IO.Path]::GetFullPath($OutputDir)
if(Test-Path $OutputDir){
    $existing=@(Get-ChildItem $OutputDir -Force -ErrorAction SilentlyContinue)
    if($existing.Count-gt0){throw "Clean-room OutputDir must be empty: $OutputDir"}
}
if($RequireCloudToken -and [string]::IsNullOrWhiteSpace($env:DOXA_CLOUD_CLIENT_TOKEN)){
    throw 'RequireCloudToken was requested but DOXA_CLOUD_CLIENT_TOKEN is not present in the current process environment.'
}
$resolvedCommit=(& git -C $repoRoot rev-parse $Commit).Trim()
if(-not$resolvedCommit){throw "Unable to resolve commit: $Commit"}

# Verify canonical caches before copying any bytes into the isolated worktree.
$deps=& (Join-Path $PSScriptRoot 'ensure-release-dependencies.ps1') -Offline -RepoRoot $repoRoot
$lock=Get-Content -Raw (Join-Path $PSScriptRoot 'release-dependencies.lock.json') | ConvertFrom-Json
$worktree=Join-Path $repoRoot ("artifacts\_cleanroom_{0}_{1}" -f ($Version-replace'[^0-9A-Za-z.-]','_'),[guid]::NewGuid().ToString('N').Substring(0,8))

try {
    & git -C $repoRoot worktree add --detach $worktree $resolvedCommit
    if($LASTEXITCODE-ne0){throw "git worktree add failed: $LASTEXITCODE"}

    foreach($d in $lock.dependencies){
        $rel=([string]$d.cache_path -replace '/','\')
        $src=Join-Path $repoRoot $rel
        $dst=Join-Path $worktree $rel
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dst)|Out-Null
        Copy-Item -LiteralPath $src -Destination $dst -Force
    }

    New-Item -ItemType Directory -Force -Path $OutputDir|Out-Null
    $package=Join-Path $worktree 'app\packaging\package.ps1'
    & $package -Version $Version -Configuration Release -OutputDir $OutputDir -OfflineDependencies
    if($LASTEXITCODE-ne0){throw "Clean-room package failed: $LASTEXITCODE"}

    $status=@(& git -C $worktree status --porcelain --untracked-files=no)
    if($status.Count-gt0){throw "Tracked files changed during clean-room release: $($status -join '; ')"}

    $prov=Get-ChildItem $OutputDir -Filter "Standivarius.Visual-$Version.provenance.json" | Select-Object -First 1
    if(-not$prov){throw 'Clean-room release did not generate provenance'}
    $p=Get-Content -Raw $prov.FullName|ConvertFrom-Json
    if($p.source.git_commit-ne$resolvedCommit){throw "Provenance commit mismatch: $($p.source.git_commit) != $resolvedCommit"}
    if($p.source.tracked_worktree_dirty){throw 'Provenance reports a dirty tracked clean-room worktree'}
    if($RequireCloudToken -and -not$p.secret_build_inputs.DOXA_CLOUD_CLIENT_TOKEN_embedded_present){throw 'Clean-room provenance does not show an embedded Doxa cloud client token.'}

    Write-Host "Clean-room release PASS: $Version @ $resolvedCommit"
    Get-ChildItem $OutputDir -File | Select-Object Name,Length,@{n='SHA256';e={(Get-FileHash $_.FullName -Algorithm SHA256).Hash}}
} finally {
    if((Test-Path $worktree) -and -not$KeepWorktree){
        & git -C $repoRoot worktree remove --force $worktree | Out-Null
    } elseif($KeepWorktree -and (Test-Path $worktree)) {
        Write-Host "Clean-room worktree retained: $worktree"
    }
}
