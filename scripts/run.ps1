$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$contentDir = Join-Path $Root "Content"

if (-not (Test-Path $contentDir)) {
    Write-Error "Content directory not found at: $contentDir"
}

$exe = Join-Path $Root "build\Release\GLEngine.exe"
if (-not (Test-Path $exe)) {
    Write-Error "Executable not found. Run: .\scripts\build.ps1 -Config Release"
}

Set-Location $Root

Write-Host "Running: $exe" -ForegroundColor Cyan
Write-Host "Working directory: $Root" -ForegroundColor Cyan
& $exe @args
