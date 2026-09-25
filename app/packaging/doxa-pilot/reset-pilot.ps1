param([switch]$DryRun)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest

$owned=@(
    (Join-Path $env:LOCALAPPDATA 'Doxa\Pilot'),
    (Join-Path $env:LOCALAPPDATA 'Standivarius.Visual')
)
$uninstallKey='HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\Standivarius.Visual'
$appPolicyKey='HKCU:\Software\Standivarius\Visual'

function Say([string]$Text){Write-Host "[Doxa reset] $Text"}
Say 'Stopping Visual if it is running.'
if(-not$DryRun){Get-Process visual_app -ErrorAction SilentlyContinue|Stop-Process -Force -ErrorAction SilentlyContinue}

$entry=Get-ItemProperty $uninstallKey -ErrorAction SilentlyContinue
$uninstaller=$null;$uninstallArgs=''
if($entry -and $entry.QuietUninstallString){
    $raw=[string]$entry.QuietUninstallString
    if($raw -match '^"([^"]+)"\s*(.*)$'){$uninstaller=$matches[1];$uninstallArgs=$matches[2]}
    else{$parts=$raw.Split(' ',2);$uninstaller=$parts[0];if($parts.Count-gt1){$uninstallArgs=$parts[1]}}
}elseif(Test-Path (Join-Path $env:LOCALAPPDATA 'Standivarius.Visual\Update.exe')){
    $uninstaller=Join-Path $env:LOCALAPPDATA 'Standivarius.Visual\Update.exe';$uninstallArgs='--uninstall --silent'
}

if($uninstaller){
    Say "Using the application's own silent uninstall: $uninstaller"
    if(-not$DryRun){
        $p=Start-Process -FilePath $uninstaller -ArgumentList $uninstallArgs -Wait -PassThru
        if($p.ExitCode-ne0){throw "Visual uninstall failed: $($p.ExitCode)"}
        Start-Sleep -Seconds 1
    }
}else{Say 'Visual uninstall entry is already absent.'}

foreach($path in $owned){
    if(Test-Path $path){
        Say "Removing owned test state: $path"
        if(-not$DryRun){Remove-Item $path -Recurse -Force -ErrorAction Stop}
    }
}
foreach($key in @($appPolicyKey,$uninstallKey)){
    if(Test-Path $key){
        Say "Removing owned registry state: $key"
        if(-not$DryRun){Remove-Item $key -Recurse -Force -ErrorAction Stop}
    }
}

$shortcutRoots=@((Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs'),[Environment]::GetFolderPath('Desktop'))
foreach($root in $shortcutRoots){
    foreach($name in @('Visual Alpha.lnk','Visual.lnk')){
        $path=Join-Path $root $name
        if(Test-Path $path){Say "Removing leftover shortcut: $path";if(-not$DryRun){Remove-Item $path -Force}}
    }
}

if($DryRun){Say 'Dry run complete. No changes were made.';exit 0}
$left=@()
foreach($path in $owned){if(Test-Path $path){$left+=$path}}
if(Test-Path $uninstallKey){$left+=$uninstallKey}
if($left.Count){throw ('Reset incomplete: '+($left -join ', '))}
Say 'Reset complete. Visual/Doxa pilot state is removed; shared runtimes and vendor/GPU drivers were left untouched.'
