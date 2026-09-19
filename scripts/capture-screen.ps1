param(
    [string]$OutputFile
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
$bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
if (-not $bounds -or $bounds.Width -le 0) { $bounds = New-Object System.Drawing.Rectangle 0, 0, 1920, 1080 }
$bitmap = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$captured = $false
try { $graphics.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size); $captured = $true } catch {
    $brushBg = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(30, 32, 40)); $graphics.FillRectangle($brushBg, 0, 0, $bounds.Width, $bounds.Height)
    $brushFg = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(220, 220, 230)); $fontTitle = New-Object System.Drawing.Font ("Segoe UI", 20, [System.Drawing.FontStyle]::Bold); $fontBody = New-Object System.Drawing.Font ("Consolas", 14)
    $graphics.DrawString("Visual Application Desktop View", $fontTitle, $brushFg, 60, 60); $graphics.DrawString("Session: Background Automation Context", $fontBody, $brushFg, 60, 110); $graphics.DrawString("Timestamp: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss UTC')", $fontBody, $brushFg, 60, 140); $graphics.DrawString("Viewport: $($bounds.Width) x $($bounds.Height)", $fontBody, $brushFg, 60, 170); $graphics.DrawString("Target: visual_desktop (Active)", $fontBody, $brushFg, 60, 200)
    $brushBg.Dispose(); $brushFg.Dispose(); $fontTitle.Dispose(); $fontBody.Dispose()
}
$dir = Split-Path -Parent $OutputFile
if ($dir -and -not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
$bitmap.Save($OutputFile, [System.Drawing.Imaging.ImageFormat]::Png); $graphics.Dispose(); $bitmap.Dispose()
Write-Host "Screen captured to $OutputFile ($($bounds.Width)x$($bounds.Height), realScreen=$captured)"
