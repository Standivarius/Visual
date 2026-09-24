param(
    [string]$KnowledgeRoot = (Join-Path (Split-Path -Parent $PSScriptRoot) 'knowledge\visual'),
    [string]$Output = (Join-Path $PSScriptRoot 'visual-support-knowledge.md')
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$files=Get-ChildItem $KnowledgeRoot -File -Filter '*.md' | Sort-Object Name
if(-not $files){throw "No Visual knowledge Markdown files found in $KnowledgeRoot"}
$sb=New-Object Text.StringBuilder
[void]$sb.AppendLine('# Visual Support Knowledge')
[void]$sb.AppendLine()
[void]$sb.AppendLine('Generated from reviewed source documents under `support/knowledge/visual/`. Each section preserves its source filename and content.')
[void]$sb.AppendLine()
foreach($f in $files){
    [void]$sb.AppendLine('---')
    [void]$sb.AppendLine()
    [void]$sb.AppendLine("## Source: $($f.Name)")
    [void]$sb.AppendLine()
    [void]$sb.AppendLine([IO.File]::ReadAllText($f.FullName).Trim())
    [void]$sb.AppendLine()
}
$encoding=New-Object Text.UTF8Encoding($false)
[IO.File]::WriteAllText([IO.Path]::GetFullPath($Output),$sb.ToString().TrimEnd()+"`n",$encoding)
Write-Output ([IO.Path]::GetFullPath($Output))
