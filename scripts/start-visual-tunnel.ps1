# Visual ngrok Tunnel Startup Script
# Machine: MARIUS-DELL

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
$EnvFile = Join-Path $ProjectRoot ".env"
$NgrokExe = Join-Path $ProjectRoot "tools\ngrok.exe"

if (-not (Test-Path $NgrokExe)) {
    Write-Error "ngrok binary not found at $NgrokExe"
    exit 1
}

# Parse .env
if (Test-Path $EnvFile) {
    Get-Content $EnvFile | ForEach-Object {
        $line = $_.Trim()
        if ($line -and -not $line.StartsWith("#")) {
            $parts = $line.Split("=", 2)
            if ($parts.Count -eq 2) {
                [System.Environment]::SetEnvironmentVariable($parts[0].Trim(), $parts[1].Trim(), "Process")
            }
        }
    }
}

$Port = if ($env:PORT) { $env:PORT } else { "8090" }

# Check if global or env authtoken is configured
$hasGlobalAuth = $false
try {
    $check = & $NgrokExe config check 2>&1
    if ($LASTEXITCODE -eq 0) { $hasGlobalAuth = $true }
} catch {}

$authToken = $env:NGROK_AUTHTOKEN
$domain = $env:NGROK_DOMAIN

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "  Starting Visual Dedicated ngrok Tunnel  " -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "Target Local Port : $Port"

$argsList = @("http", $Port, "--log=stdout", "--log-format=term")

if ($authToken) {
    $argsList += @("--authtoken", $authToken)
    Write-Host "Auth Token        : Loaded from .env"
} elseif (-not $hasGlobalAuth) {
    Write-Host ""
    Write-Host "Notice: No ngrok authtoken found." -ForegroundColor Yellow
    Write-Host "To authenticate ngrok, you can either:" -ForegroundColor Yellow
    Write-Host "  1. Add NGROK_AUTHTOKEN=<your-token> to $EnvFile" -ForegroundColor Yellow
    Write-Host "  2. Or run: & `"$NgrokExe`" config add-authtoken <your-token>" -ForegroundColor Yellow
    Write-Host "Get your free authtoken at: https://dashboard.ngrok.com/get-started/your-authtoken" -ForegroundColor Yellow
    Write-Host ""
}

if ($domain) {
    $argsList += @("--url", $domain)
    Write-Host "Reserved Domain   : $domain"
}

Write-Host "Starting ngrok tunnel..."
Write-Host "Inspect UI will be available at: http://127.0.0.1:4040"
Write-Host "Press Ctrl+C to terminate the tunnel."
Write-Host "=========================================" -ForegroundColor Cyan

& $NgrokExe @argsList
