param()
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$logDir = Join-Path $repo 'artifacts\toolchain-install'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$log = Join-Path $logDir 'install.log'
$status = Join-Path $logDir 'status.txt'
$stdout = Join-Path $logDir 'winget.stdout.txt'
$stderr = Join-Path $logDir 'winget.stderr.txt'
"STARTED $(Get-Date -Format o)" | Set-Content -Encoding UTF8 $status
"=== Visual Spike 1 toolchain install ===`nStarted: $(Get-Date -Format o)" | Set-Content -Encoding UTF8 $log

$winget = (Get-Command winget.exe -ErrorAction Stop).Source
$override = '--wait --quiet --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended'

try {
    "winget=$winget" | Add-Content -Encoding UTF8 $log
    "Installing Microsoft.VisualStudio.2022.BuildTools with Desktop development with C++ + recommended components" | Add-Content -Encoding UTF8 $log
    $args = @(
        'install',
        '--id','Microsoft.VisualStudio.2022.BuildTools',
        '--exact',
        '--source','winget',
        '--accept-source-agreements',
        '--accept-package-agreements',
        '--silent',
        '--disable-interactivity',
        '--override', $override
    )
    & $winget @args 1> $stdout 2> $stderr
    $exitCode = $LASTEXITCODE
    "winget_exit=$exitCode" | Add-Content -Encoding UTF8 $log
    if ($exitCode -ne 0) { throw "winget install failed with exit code $exitCode" }
    "COMPLETED $(Get-Date -Format o)" | Set-Content -Encoding UTF8 $status
    "Completed: $(Get-Date -Format o)" | Add-Content -Encoding UTF8 $log
}
catch {
    "FAILED $(Get-Date -Format o) $($_.Exception.Message)" | Set-Content -Encoding UTF8 $status
    "ERROR: $($_.Exception.ToString())" | Add-Content -Encoding UTF8 $log
    exit 1
}
