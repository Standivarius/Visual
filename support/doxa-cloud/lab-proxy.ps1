param(
    [int]$Port=8787,
    [ValidateSet('Dify','Meta')][string]$Provider='Dify',
    [string]$LabToken=$env:DOXA_LAB_TOKEN,
    [string]$DifyApiKey=$env:DIFY_API_KEY,
    [string]$DifyApiBase='https://api.dify.ai/v1',
    [string]$MetaApiKey=$env:META_API_KEY,
    [string]$MetaModel='muse-spark-1.3-contributor'
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest

if([string]::IsNullOrWhiteSpace($LabToken)){throw 'DOXA_LAB_TOKEN (or -LabToken) is required.'}
if($Provider-eq'Dify' -and [string]::IsNullOrWhiteSpace($DifyApiKey)){
    $DifyApiKey=[Environment]::GetEnvironmentVariable('DIFY_API_KEY','User')
}
if($Provider-eq'Dify' -and [string]::IsNullOrWhiteSpace($DifyApiKey)){throw 'DIFY_API_KEY is required for Provider=Dify.'}
if($Provider-eq'Meta' -and [string]::IsNullOrWhiteSpace($MetaApiKey)){throw 'META_API_KEY is required for Provider=Meta.'}

$actionsPath=Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) 'app\packaging\doxa-setup\approved-actions.json'
if(-not(Test-Path $actionsPath -PathType Leaf)){throw "Approved actions not found: $actionsPath"}
$actions=Get-Content $actionsPath -Raw|ConvertFrom-Json
$allowed=@($actions.ai_actions|ForEach-Object{[string]$_.id})

function Write-JsonResponse($context,[int]$status,$object){
    $json=$object|ConvertTo-Json -Depth 12 -Compress
    $bytes=[Text.Encoding]::UTF8.GetBytes($json)
    $context.Response.StatusCode=$status
    $context.Response.ContentType='application/json; charset=utf-8'
    $context.Response.ContentLength64=$bytes.Length
    $context.Response.OutputStream.Write($bytes,0,$bytes.Length)
    $context.Response.OutputStream.Close()
}

function Read-Body($request){
    if($request.ContentLength64 -gt 65536){throw 'Request body exceeds 64 KiB.'}
    $reader=New-Object IO.StreamReader($request.InputStream,$request.ContentEncoding)
    try{return $reader.ReadToEnd()}finally{$reader.Dispose()}
}

function Normalize-Plan([string]$text){
    $text=$text.Trim()
    if($text.StartsWith('```')){$text=$text -replace '^```(?:json)?\s*','' -replace '\s*```$',''}
    try{return $text|ConvertFrom-Json}catch{}
    $start=$text.IndexOf('{');$end=$text.LastIndexOf('}')
    if($start-ge0 -and $end-gt$start){
        try{return $text.Substring($start,$end-$start+1)|ConvertFrom-Json}catch{}
    }
    throw 'Planner returned no valid JSON object.'
}

function New-Prompt($state){
    $stateJson=$state|ConvertTo-Json -Depth 10 -Compress
    $allowedJson=$allowed|ConvertTo-Json -Compress
    return @"
You are the exception planner for the controlled Doxa Windows installer.
The normal installer is deterministic. You are called only when the deterministic checks cannot explain the remaining failure.
Choose exactly one action from the allowed list. Never invent commands, scripts, registry changes, driver replacements, security bypasses, downloads, or additional actions.
If enterprise policy is likely involved, choose escalate_it. If evidence is insufficient, prefer a diagnostic action or collect_support_bundle.

Machine state:
$stateJson

Allowed actions:
$allowedJson

Return JSON only with exactly these fields:
{"decision":"diagnose|escalate","action":"one allowed action id","reason":"short plain-English reason","confidence":"low|medium|high","needs_it":true|false}
"@
}

function Invoke-Dify([string]$prompt,[string]$user){
    $headers=@{Authorization="Bearer $DifyApiKey";'Content-Type'='application/json'}
    $body=@{inputs=@{};query=$prompt;response_mode='blocking';conversation_id='';user=$user}|ConvertTo-Json -Depth 8
    $r=Invoke-RestMethod -Method Post -Uri ($DifyApiBase.TrimEnd('/')+'/chat-messages') -Headers $headers -Body $body -TimeoutSec 90
    if(-not($r.PSObject.Properties.Name -contains 'answer')){throw 'Dify response did not contain answer.'}
    return [string]$r.answer
}

function Invoke-Meta([string]$prompt){
    $headers=@{Authorization="Bearer $MetaApiKey";'Content-Type'='application/json'}
    $body=@{model=$MetaModel;messages=@(@{role='user';content=$prompt});temperature=0}|ConvertTo-Json -Depth 8
    $r=Invoke-RestMethod -Method Post -Uri 'https://api.meta.ai/v1/chat/completions' -Headers $headers -Body $body -TimeoutSec 90
    return [string]$r.choices[0].message.content
}

$listener=[Net.HttpListener]::new()
$listener.Prefixes.Add("http://127.0.0.1:$Port/")
$listener.Start()
Write-Host "DOXA_LAB_PROXY_READY http://127.0.0.1:$Port provider=$Provider"
try{
    while($listener.IsListening){
        $context=$listener.GetContext()
        try{
            $path=$context.Request.Url.AbsolutePath
            if($context.Request.HttpMethod-eq'GET' -and $path-eq'/health'){
                Write-JsonResponse $context 200 ([pscustomobject]@{ok=$true;service='doxa-lab-proxy';provider=$Provider;schema_version='1'})
                continue
            }
            if($context.Request.HttpMethod-ne'POST' -or $path-ne'/v1/plan'){
                Write-JsonResponse $context 404 ([pscustomobject]@{error='not_found'});continue
            }
            $auth=[string]$context.Request.Headers['Authorization']
            if($auth-ne"Bearer $LabToken"){
                Write-JsonResponse $context 401 ([pscustomobject]@{error='unauthorized'});continue
            }
            $raw=Read-Body $context.Request
            $input=$raw|ConvertFrom-Json
            if(-not($input.PSObject.Properties.Name -contains 'state')){throw 'Request must contain state.'}
            $user='doxa-installer'
            if($input.PSObject.Properties.Name -contains 'client_id' -and $input.client_id){$user='doxa-'+([string]$input.client_id -replace '[^A-Za-z0-9_-]','')}
            $prompt=New-Prompt $input.state
            $text=if($Provider-eq'Dify'){Invoke-Dify $prompt $user}else{Invoke-Meta $prompt}
            $plan=Normalize-Plan $text
            $action=[string]$plan.action
            if($allowed -notcontains $action){throw "Planner selected action outside allowlist: $action"}
            $decision=[pscustomobject]@{
                outcome='ai_plan'
                action=$action
                reason=[string]$plan.reason
                used_ai=$true
                confidence=[string]$plan.confidence
                needs_it=[bool]$plan.needs_it
                provider=$Provider.ToLowerInvariant()
            }
            Write-JsonResponse $context 200 ([pscustomobject]@{schema_version='1';decision=$decision})
        }catch{
            try{Write-JsonResponse $context 502 ([pscustomobject]@{error='planner_failed';message=$_.Exception.Message})}catch{}
        }
    }
}finally{
    $listener.Stop();$listener.Close()
}
