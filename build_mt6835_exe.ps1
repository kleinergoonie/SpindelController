param(
    [string]$Input = "mt6835_magnet_live.ps1",
    [string]$Output = "mt6835_magnet_live.exe",
    [switch]$NoConsole
)

Set-StrictMode -Version Latest
$cwd = Get-Location
$inputPath = Join-Path $cwd.Path $Input
$outputPath = Join-Path $cwd.Path $Output

if (-not (Test-Path $inputPath)) {
    Write-Error "Input file not found: $inputPath"
    exit 2
}

# Ensure ps2exe is available
if (-not (Get-Command Invoke-ps2exe -ErrorAction SilentlyContinue)) {
    Write-Output "ps2exe nicht gefunden. Versuche Installation aus PSGallery..."
    try {
        Install-Module -Name ps2exe -Scope CurrentUser -Force -AllowClobber -ErrorAction Stop
    } catch {
        Write-Error "Fehler beim Installieren von ps2exe: $_"
        exit 3
    }
}

# Convert to EXE
try {
    $noConsoleFlag = $NoConsole.IsPresent
    if ($noConsoleFlag) {
        Invoke-ps2exe -inputFile $inputPath -outputFile $outputPath -noConsole
    } else {
        Invoke-ps2exe -inputFile $inputPath -outputFile $outputPath
    }
    Write-Output "Erstellt: $outputPath"
} catch {
    Write-Error "Konvertierung fehlgeschlagen: $_"
    exit 4
}
