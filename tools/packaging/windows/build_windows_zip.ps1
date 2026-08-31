param(
    [string]$Version = "0.1.0",
    [string]$OutputDir = "$PSScriptRoot/../../../dist",
    [string]$BuildDir = "$PSScriptRoot/../../../build/windows-msvc-release"
)

$ErrorActionPreference = "Stop"

$SourceDir = (Resolve-Path "$PSScriptRoot/../../..").Path
$OutputDir = (New-Item -ItemType Directory -Force -Path $OutputDir).FullName
$StageName = "ArcadeBlocksII-$Version-windows-x64"
$StageDir = Join-Path $env:TEMP $StageName

if (Test-Path $StageDir) {
    Remove-Item -Recurse -Force $StageDir
}
New-Item -ItemType Directory -Force -Path $StageDir | Out-Null

Write-Host "==> Staging Windows package in $StageDir..."

# Find executable
$ExePath = Join-Path $BuildDir "ArcadeBlocksII.exe"
if (-not (Test-Path $ExePath)) {
    $ExePath = Join-Path $BuildDir "Release/ArcadeBlocksII.exe"
}
if (-not (Test-Path $ExePath)) {
    throw "ArcadeBlocksII.exe not found in $BuildDir"
}
Copy-Item $ExePath $StageDir

# Copy any DLLs in the build tree
Get-ChildItem -Path $BuildDir -Filter "*.dll" -Recurse | ForEach-Object {
    Copy-Item $_.FullName $StageDir -Force
}

# Copy assets
Copy-Item -Recurse (Join-Path $SourceDir "assets") (Join-Path $StageDir "assets")
Copy-Item (Join-Path $SourceDir "README.md") $StageDir

$ZipPath = Join-Path $OutputDir "$StageName.zip"
if (Test-Path $ZipPath) {
    Remove-Item -Force $ZipPath
}

Write-Host "==> Creating Windows ZIP: $ZipPath..."
Compress-Archive -Path "$StageDir/*" -DestinationPath $ZipPath -Force

Remove-Item -Recurse -Force $StageDir
Write-Host "==> Successfully created $ZipPath"
