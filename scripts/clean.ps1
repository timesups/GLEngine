$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

$targets = @(
    "build",
    "build-clang-test",
    "cmake-build",
    ".vs",
    ".cache",
    "imgui.ini"
)

foreach ($target in $targets) {
    if (Test-Path $target) {
        Remove-Item -Recurse -Force $target
        Write-Host "Removed: $target" -ForegroundColor Green
    }
}

Write-Host "Clean complete." -ForegroundColor Cyan
