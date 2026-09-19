# Visual Repository Bridge Startup Script
# Machine: MARIUS-DELL

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
$EnvFile = Join-Path $ProjectRoot ".env"

if (-not (Test-Path $EnvFile)) {
    Write-Error "Configuration file not found: $EnvFile"
    exit 1
}

# Parse .env
Get-Content $EnvFile | ForEach-Object {
    $line = $_.Trim()
    if ($line -and -not $line.StartsWith("#")) {
        $parts = $line.Split("=", 2)
        if ($parts.Count -eq 2) {
            [System.Environment]::SetEnvironmentVariable($parts[0].Trim(), $parts[1].Trim(), "Process")
        }
    }
}

# Locate Node.js executable
$NodeExe = $null
if (Get-Command node -ErrorAction SilentlyContinue) {
    $NodeExe = "node"
} elseif (Test-Path "C:\Users\DELL\AppData\Local\OpenAI\Codex\runtimes\cua_node\4004642ff3fabdc7\bin\node.exe") {
    $NodeExe = "C:\Users\DELL\AppData\Local\OpenAI\Codex\runtimes\cua_node\4004642ff3fabdc7\bin\node.exe"
} else {
    Write-Error "Node.js executable could not be found."
    exit 1
}

$Port = if ($env:PORT) { $env:PORT } else { "8090" }
$HostAddr = if ($env:HOST) { $env:HOST } else { "127.0.0.1" }
$ServerScript = Join-Path $ProjectRoot "bridge\server.mjs"

# Stop any existing process on the port for a clean restart
$existingConns = Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue
foreach ($conn in $existingConns) {
    if ($conn.OwningProcess -and $conn.OwningProcess -ne $PID) {
        Write-Host "Stopping previous bridge process on port $Port (PID: $($conn.OwningProcess))..." -ForegroundColor Yellow
        Stop-Process -Id $conn.OwningProcess -Force -ErrorAction SilentlyContinue
        Start-Sleep -Milliseconds 500
    }
}

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "  Starting Visual Repository Bridge      " -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "Project Root : $ProjectRoot"
Write-Host "Workspace    : $env:WORKSPACE_ROOT"
Write-Host "Local URL    : http://${HostAddr}:${Port}"
Write-Host "OpenAPI URL  : http://${HostAddr}:${Port}/openapi_chatgpt_compact.json"
Write-Host "Auth Token   : Loaded from $EnvFile"
Write-Host "=========================================" -ForegroundColor Cyan

& $NodeExe $ServerScript
