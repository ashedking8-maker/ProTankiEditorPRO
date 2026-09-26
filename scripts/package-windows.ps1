param([string]$BuildDir = "build")
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$build = [IO.Path]::GetFullPath((Join-Path $root $BuildDir))
$stage = Join-Path $build "package"
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
cmake --install $build --config Release --prefix $stage
if ($LASTEXITCODE -ne 0) { throw "Install staging failed" }
$makensis = (Get-Command makensis.exe -ErrorAction SilentlyContinue).Source
if (-not $makensis) { $makensis = Join-Path ${env:ProgramFiles(x86)} "NSIS\makensis.exe" }
if (!(Test-Path $makensis)) { throw "NSIS makensis.exe was not found" }
$icon = Join-Path $root "assets\GTanksNextEditor.ico"
$outfile = Join-Path $build "ProTankiEditorPRO-0.5.21-Setup.exe"
& $makensis "/DINPUT_DIR=$stage" "/DOUTPUT_FILE=$outfile" "/DICON_FILE=$icon" (Join-Path $root "installer\SimpleInstaller.nsi")
if ($LASTEXITCODE -ne 0 -or !(Test-Path $outfile)) { throw "NSIS installer creation failed" }
Push-Location $build
try {
  cpack -C Release -G ZIP
  if ($LASTEXITCODE -ne 0) { throw "Portable ZIP failed" }
} finally { Pop-Location }
$portable = Join-Path $build "ProTankiEditorPRO-0.5.21-Portable.zip"
if (!(Test-Path $portable)) { throw "Portable ZIP was not created: $portable" }
Write-Host "Created $outfile"
Write-Host "Created $portable"
