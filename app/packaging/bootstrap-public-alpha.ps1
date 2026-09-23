param(
    [string]$RemoteUrl = 'https://github.com/Standivarius/Visual.git',
    [string]$Branch = 'master',
    [string]$Version = '0.1.0-alpha.1',
    [switch]$NoTag
)
$ErrorActionPreference='Stop'

$repoRoot=Resolve-Path (Join-Path $PSScriptRoot '..\..')
Push-Location $repoRoot
try {
    if(-not(Test-Path '.git')){throw "Not a Git repository: $repoRoot"}
    if($Version -notmatch '^\d+\.\d+\.\d+-alpha\.\d+$'){
        throw "Version must match MAJOR.MINOR.PATCH-alpha.N; got: $Version"
    }

    $existingRemote=(& git remote get-url origin 2>$null)
    if($LASTEXITCODE-ne0 -or -not $existingRemote){
        & git remote add origin $RemoteUrl
        if($LASTEXITCODE-ne0){throw 'Unable to add origin remote'}
    } elseif($existingRemote.TrimEnd('/') -ne $RemoteUrl.TrimEnd('/')) {
        throw "Existing origin points to '$existingRemote', expected '$RemoteUrl'. Refusing to overwrite it."
    }

    $currentBranch=(& git branch --show-current).Trim()
    if($currentBranch -ne $Branch){
        throw "Current branch is '$currentBranch', expected '$Branch'. Refusing to switch branches automatically."
    }

    # Only these paths are part of the first public Visual alpha surface.
    # app/src and app/tests contain source/test code only; packaging/support
    # documents/scripts are explicitly enumerated to avoid sweeping in later local output.
    $publicPaths=@(
        '.gitignore',
        '.config/dotnet-tools.json',
        '.github/workflows/release-alpha.yml',
        'app/CMakeLists.txt',
        'app/build.ps1',
        'app/app.manifest',
        'app/README.md',
        'app/version.cmake',
        'app/src',
        'app/tests',
        'app/packaging/bootstrap-public-alpha.ps1',
        'app/packaging/LOCAL_AGENT_PUBLISH_PROMPT.md',
        'app/packaging/package.ps1',
        'app/packaging/README.md',
        'app/packaging/RELEASE_NOTES.md',
        'app/packaging/support-bundle.ps1',
        'support/dify/evaluation-cases.md',
        'support/dify/README.md',
        'support/dify/SETUP.md',
        'support/dify/system-prompt.md',
        'support/knowledge/README.md',
        'support/knowledge/visual/diagnostics.md',
        'support/knowledge/visual/installation.md',
        'support/knowledge/visual/known-issues.md',
        'support/knowledge/visual/product-overview.md',
        'support/knowledge/visual/troubleshooting/display-topology-change.md',
        'support/knowledge/visual/troubleshooting/tracking-problem.md'
    )

    & git add -- $publicPaths
    if($LASTEXITCODE-ne0){throw 'git add failed'}

    $staged=@(& git diff --cached --name-only)
    if($LASTEXITCODE-ne0){throw 'Unable to inspect staged files'}
    if($staged.Count-eq0){throw 'Nothing staged for the Visual alpha bootstrap commit'}

    # Git normalizes paths to forward slashes. Permit the two intentional source trees
    # plus exact files above; reject anything else already staged.
    $allowedExact=@($publicPaths | Where-Object { $_ -notin @('app/src','app/tests') })
    $allowedPrefixes=@('app/src/','app/tests/')
    foreach($file in $staged){
        $allowed=($allowedExact -contains $file)
        if(-not $allowed){
            foreach($prefix in $allowedPrefixes){
                if($file.StartsWith($prefix)){ $allowed=$true; break }
            }
        }
        if(-not $allowed){throw "Unexpected staged path '$file'. Refusing to commit."}
    }

    Write-Host 'Staged public Visual files:'
    $staged | ForEach-Object { Write-Host "  $_" }

    & git commit -m 'chore: bootstrap Visual alpha distribution'
    if($LASTEXITCODE-ne0){throw 'git commit failed'}

    & git push -u origin $Branch
    if($LASTEXITCODE-ne0){throw "git push origin $Branch failed. Authenticate GitHub locally, then rerun."}

    if(-not $NoTag){
        $tag="v$Version"
        $existingTag=& git tag --list $tag
        if($existingTag){
            throw "Tag $tag already exists locally. Inspect it before retrying; the commit has already been pushed."
        }
        & git tag -a $tag -m "Visual $Version"
        if($LASTEXITCODE-ne0){throw "Unable to create $tag"}
        & git push origin $tag
        if($LASTEXITCODE-ne0){throw "Unable to push $tag"}
        Write-Host "Pushed $tag. GitHub Actions should now build and publish the Visual alpha prerelease."
    }

    Write-Host "Repository: https://github.com/Standivarius/Visual"
    Write-Host "Releases:   https://github.com/Standivarius/Visual/releases"
}
finally {
    Pop-Location
}
