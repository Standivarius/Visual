param([switch]$Clear)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
if($Clear){
    [Environment]::SetEnvironmentVariable('DIFY_API_KEY',$null,'User')
    Write-Host 'DIFY_API_KEY cleared from the current Windows user environment.'
    exit 0
}
$secure=Read-Host 'Paste the Dify Service API key for Visual Support Alpha' -AsSecureString
$ptr=[Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
try{$plain=[Runtime.InteropServices.Marshal]::PtrToStringBSTR($ptr)}finally{[Runtime.InteropServices.Marshal]::ZeroFreeBSTR($ptr)}
if([string]::IsNullOrWhiteSpace($plain)){throw 'No API key was entered.'}
[Environment]::SetEnvironmentVariable('DIFY_API_KEY',$plain,'User')
$plain=$null
Write-Host 'DIFY_API_KEY stored for the current Windows user. The value was not printed.'
