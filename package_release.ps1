# PowerShell script to build Release and create a portable one-click Windows folder:
# dist\RobotPathPlanning\RobotPathPlanning.exe + DLLs + assets + scenarios + README.txt
$ErrorActionPreference = "Stop"

Write-Host "=== Configuring CMake Release Build (MinGW Makefiles) ===" -ForegroundColor Cyan
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release

Write-Host "=== Compiling RobotPathPlanning & Core Tests ===" -ForegroundColor Cyan
cmake --build build --config Release -j

Write-Host "=== Running C++ Core Verification Tests ===" -ForegroundColor Cyan
ctest --test-dir build --output-on-failure

$ReleaseDir = Join-Path $PSScriptRoot "release\RobotPathPlanning"
if (Test-Path $ReleaseDir) { Remove-Item $ReleaseDir -Recurse -Force }
New-Item -ItemType Directory -Path $ReleaseDir | Out-Null

Copy-Item "build\RobotPathPlanning.exe" $ReleaseDir
Copy-Item "build\*.dll" $ReleaseDir -ErrorAction SilentlyContinue
if (Test-Path "assets") { Copy-Item "assets" $ReleaseDir -Recurse }
Copy-Item "scenarios" $ReleaseDir -Recurse
Copy-Item "README.txt" $ReleaseDir

# Copy MinGW runtime DLLs if dynamically linked
$GxxPath = (Get-Command g++).Source
$MinGWBin = Split-Path $GxxPath -Parent
foreach ($dll in @("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")) {
    $cand = Join-Path $MinGWBin $dll
    if (Test-Path $cand) {
        Copy-Item $cand $ReleaseDir
    }
}

Write-Host "`n[DONE] Portable release package created at: $ReleaseDir" -ForegroundColor Green
Write-Host "Double-click RobotPathPlanning.exe inside that folder to launch!" -ForegroundColor Green
