param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

function Find-Ninja {
    $cmd = Get-Command ninja -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $fallbacks = @(
        "$env:ProgramFiles\Ninja\ninja.exe"
        "$env:LOCALAPPDATA\Microsoft\WinGet\Links\ninja.exe"
    )
    foreach ($path in $fallbacks) {
        if (Test-Path $path) { return $path }
    }

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild `
            -property installationPath 2>$null
        if ($vsPath) {
            $ninja = Join-Path $vsPath "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
            if (Test-Path $ninja) { return $ninja }
        }
    }

    throw "Ninja not found. Install with: winget install Ninja-build.Ninja"
}

function Find-LlvmBin {
    if ($env:LLVM_ROOT) {
        $bin = Join-Path $env:LLVM_ROOT "bin"
        if (Test-Path (Join-Path $bin "clang.exe")) { return $bin }
    }
    $default = "C:\Program Files\LLVM\bin"
    if (Test-Path (Join-Path $default "clang.exe")) { return $default }
    throw "LLVM/Clang not found. Install with: winget install LLVM.LLVM"
}

$llvmBin = Find-LlvmBin
$ninja = Find-Ninja
$env:PATH = "$llvmBin;$env:PATH"

if ($Clean -and (Test-Path "build")) {
    Remove-Item -Recurse -Force "build"
}

$configureArgs = @(
    "-S", "."
    "-B", "build"
    "-G", "Ninja"
    "-DCMAKE_BUILD_TYPE=$Config"
    "-DCMAKE_TOOLCHAIN_FILE=cmake/toolchain/clang-windows.cmake"
    "-DCMAKE_MAKE_PROGRAM=$ninja"
)

Write-Host ">> cmake $($configureArgs -join ' ')" -ForegroundColor Cyan
& cmake @configureArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ">> cmake --build build" -ForegroundColor Cyan
& cmake --build build
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "Built: build\$Config\GLEngineNew.exe" -ForegroundColor Green
