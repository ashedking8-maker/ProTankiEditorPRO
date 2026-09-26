$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root "build-release"

Write-Host "[1/4] Configuring x64 Release build..."
cmake -S $root -B $build -G "Visual Studio 17 2022" -A x64

Write-Host "[2/4] Building GTanks Next Editor..."
cmake --build $build --config Release --parallel

Write-Host "[3/3] Packaging installer and portable ZIP..."
& (Join-Path $PSScriptRoot "package-windows.ps1") -BuildDir "build-release"
