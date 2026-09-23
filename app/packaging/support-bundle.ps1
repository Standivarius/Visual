param(
    [ValidateSet('Debug','Release','RelWithDebInfo','MinSizeRel')][string]$Configuration='Release',
    [string]$TelemetryPath='',
    [string]$OutputDir=''
)
$ErrorActionPreference='Stop'

$appRoot=Split-Path -Parent $PSScriptRoot
if(-not $OutputDir){$OutputDir=Join-Path $PSScriptRoot 'support-output'}
New-Item -ItemType Directory -Force -Path $OutputDir|Out-Null

$diagnosticsExe=Join-Path $appRoot "build\$Configuration\visual_diagnostics.exe"
if(-not(Test-Path $diagnosticsExe)){throw "Diagnostics executable not found: $diagnosticsExe. Build Visual first."}

$stamp=Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
$temp=Join-Path $OutputDir "visual-support-$stamp"
$zip=Join-Path $OutputDir "visual-support-$stamp.zip"
New-Item -ItemType Directory -Force -Path $temp|Out-Null

$resolvedTelemetry=$null
$telemetryDescription='- no runtime telemetry included'
$expectedHeader='frame,qpc,zoom,tracking,poi_present,poi_source,locator_visible,viewport_action,view_left,view_top,view_right,view_bottom,present_hr,pointer_age_ms,uia_snapshot_age_ms,uia_caret_present,uia_focus_present,selected_poi_age_ms,selected_left,selected_top,selected_right,selected_bottom'
$expectedFieldCount=22
$numericFieldPattern='^-?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?$'
$eventPattern='^# event=[A-Za-z0-9_.:-]+$'

if($TelemetryPath){
    $resolvedTelemetry=Get-Item (Resolve-Path $TelemetryPath -ErrorAction Stop)
    if($resolvedTelemetry.PSIsContainer){throw 'TelemetryPath must be a file, not a directory.'}
    if($resolvedTelemetry.Extension -ne '.csv'){throw 'TelemetryPath must be a Visual CSV telemetry file.'}
    if($resolvedTelemetry.Length -gt 100MB){throw 'TelemetryPath exceeds the 100 MB support-bundle safety limit.'}

    $lineNumber=0
    foreach($line in [IO.File]::ReadLines($resolvedTelemetry.FullName)){
        $lineNumber++
        if($lineNumber -eq 1){
            if($line -ne $expectedHeader){throw 'TelemetryPath does not match the expected Visual telemetry CSV schema.'}
            continue
        }
        if($line.Length -gt 4096){throw "TelemetryPath line $lineNumber exceeds the expected Visual telemetry line length."}
        if($line -match $eventPattern){continue}
        if([string]::IsNullOrWhiteSpace($line)){throw "TelemetryPath contains an unexpected blank line at $lineNumber."}

        $fields=$line.Split(',')
        if($fields.Count -ne $expectedFieldCount){throw "TelemetryPath line $lineNumber does not contain $expectedFieldCount Visual telemetry fields."}
        foreach($field in $fields){
            if($field -notmatch $numericFieldPattern){throw "TelemetryPath line $lineNumber contains a non-numeric Visual telemetry field."}
        }
    }
    if($lineNumber -lt 1){throw 'TelemetryPath is empty.'}
    $telemetryDescription='- visual-telemetry.csv: validated Visual runtime telemetry explicitly supplied by the user; included verbatim'
}

try{
    $diag=Join-Path $temp 'diagnostics.json'
    & $diagnosticsExe --out $diag
    if($LASTEXITCODE-ne0){throw "visual_diagnostics.exe failed: $LASTEXITCODE"}

    if($resolvedTelemetry){
        Copy-Item -LiteralPath $resolvedTelemetry.FullName (Join-Path $temp 'visual-telemetry.csv')
    }

    @"
Visual Support Bundle
Created: $([DateTime]::UtcNow.ToString('o'))

Contents:
- diagnostics.json: Visual version, Windows build, display geometry/mode/DPI and GPU-model metadata
$telemetryDescription

Privacy boundary:
This bundle does not intentionally collect screenshots, document/window/browser contents, usernames, arbitrary file listings, passwords, tokens, or environment-variable dumps.
Display and GPU model metadata is included because it is relevant to magnification/display troubleshooting.
If runtime telemetry is supplied, that CSV is copied verbatim only after validating the exact current Visual telemetry schema, frame/event row shape, numeric frame fields and a size limit.
"@ | Set-Content -Encoding UTF8 (Join-Path $temp 'README.txt')

    Compress-Archive -Path (Join-Path $temp '*') -DestinationPath $zip -CompressionLevel Optimal -Force
}finally{
    Remove-Item -Recurse -Force $temp -ErrorAction SilentlyContinue
}

Write-Host "Support bundle created: $zip"
Get-Item $zip | Select-Object FullName,Length,LastWriteTime
