# Build-Skript fuer SpindelController
# Baut lokal in C:\Build\SpindelController und kopiert Artefakte zurueck aufs NAS.

$SourceDir  = "z:\SpindelController\SpindelController"
$BuildDir   = "C:\Build\SpindelController"
$NasOut     = "$SourceDir\bin"

# --- CMake Konfigurieren (nur beim ersten Mal oder wenn CMakeCache fehlt) ------
if (-not (Test-Path "$BuildDir\CMakeCache.txt")) {
    Write-Host "--- CMake konfigurieren ---" -ForegroundColor Cyan
    cmake $SourceDir -G Ninja -B $BuildDir
    if ($LASTEXITCODE -ne 0) { Write-Error "CMake Konfiguration fehlgeschlagen"; exit 1 }
}

# --- Bauen -------------------------------------------------------------------
Write-Host "--- Bauen ---" -ForegroundColor Cyan
cmake --build $BuildDir --parallel
if ($LASTEXITCODE -ne 0) { Write-Error "Build fehlgeschlagen"; exit 1 }

# --- Artefakte auf NAS kopieren ----------------------------------------------
Write-Host "--- Upload auf NAS ---" -ForegroundColor Cyan
New-Item -ItemType Directory -Path $NasOut -Force | Out-Null

$Artifacts = @("SpindelController.uf2", "SpindelController.elf", "SpindelController.bin", "SpindelController.hex")
foreach ($file in $Artifacts) {
    $src = Join-Path $BuildDir $file
    if (Test-Path $src) {
        Copy-Item $src -Destination $NasOut -Force
        Write-Host "  Kopiert: $file -> $NasOut" -ForegroundColor Green
    }
}

Write-Host "--- Fertig ---" -ForegroundColor Green
