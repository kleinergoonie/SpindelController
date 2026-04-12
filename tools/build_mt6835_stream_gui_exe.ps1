# Packaging script for mt6835_stream_gui.ps1 -> EXE using ps2exe
# Requires: Install-Module -Name ps2exe -Scope CurrentUser

$scriptPath = Join-Path $PSScriptRoot 'mt6835_stream_gui.ps1'
$outPath = Join-Path $PSScriptRoot '..\\bin\\mt6835_stream_gui.exe'

if (-not (Test-Path $scriptPath)) {
    Write-Error "Script not found: $scriptPath"
    exit 1
}

# Try to import ps2exe
try {
    Import-Module ps2exe -ErrorAction Stop
} catch {
    Write-Error "ps2exe module not available. Install with: Install-Module -Name ps2exe -Scope CurrentUser"
    exit 1
}

# Build EXE (GUI) - no console window
Invoke-ps2exe $scriptPath $outPath -noConsole -icon $null
Write-Output "Wrote EXE: $outPath"
