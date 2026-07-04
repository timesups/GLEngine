$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

$exe = Join-Path $Root "build\Debug\GLEngineNew.exe"
if (-not (Test-Path $exe)) {
    $exe = Join-Path $Root "build\Release\GLEngineNew.exe"
}
if (-not (Test-Path $exe)) {
    Write-Error "Executable not found. Run .\scripts\build.ps1 first."
}

Write-Host "Running: $exe" -ForegroundColor Cyan
Write-Host "Working directory: $Root" -ForegroundColor Cyan
& $exe @args
