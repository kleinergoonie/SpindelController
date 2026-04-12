<#
make_exe.ps1

Erzeugt Windows-Exe-Dateien aus den PowerShell-Monitor-Skripten
Benutzt das PS2EXE-Modul (https://github.com/MScholtes/PS2EXE)

Usage:
  PowerShell -NoProfile -ExecutionPolicy Bypass -File .\make_exe.ps1

#>

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$binDir = Join-Path $repoRoot 'bin'
if (-not (Test-Path $binDir)) { New-Item -ItemType Directory -Path $binDir | Out-Null }

$scripts = @(
    'as5600_monitor.ps1',
    'mt6835_monitor.ps1',
    'encoder_monitor.ps1'
)

function Ensure-PS2EXE {
    # Check for PS2EXE (module name may be PS2EXE or Ps2Exe)
    if (Get-Command -Name Invoke-PS2EXE -ErrorAction SilentlyContinue) { return }
    if (Get-Command -Name ps2exe -ErrorAction SilentlyContinue) { return }

    Write-Host 'PS2EXE not found. Attempting to install from PSGallery (requires internet and admin/user install rights)...' -ForegroundColor Yellow
    try {
        Install-Module -Name PS2EXE -Scope CurrentUser -Force -AllowClobber
    }
    catch {
        Write-Warning "Install-Module failed: $($_.Exception.Message)"
        throw
    }

    if (-not (Get-Command -Name Invoke-PS2EXE -ErrorAction SilentlyContinue) -and -not (Get-Command -Name ps2exe -ErrorAction SilentlyContinue)) {
        throw 'PS2EXE not available after install.'
    }
}

function Convert-ScriptToExe($scriptPath, $outExe) {
    Write-Host "Converting $scriptPath -> $outExe"
    $invoke = Get-Command -Name Invoke-PS2EXE -ErrorAction SilentlyContinue
    if ($invoke) {
        Invoke-PS2EXE -InputFile $scriptPath -OutputFile $outExe -NoConsole
        return
    }
    $alias = Get-Command -Name ps2exe -ErrorAction SilentlyContinue
    if ($alias) {
        & $alias.Source $scriptPath $outExe -noConsole
        return
    }
    throw 'No ps2exe command available.'
}

try {
    Ensure-PS2EXE
}
catch {
    Write-Error 'PS2EXE is not available and could not be installed. Please install the PS2EXE module manually and rerun this script.'
    exit 1
}

foreach ($s in $scripts) {
    $src = Join-Path $repoRoot $s
    if (-not (Test-Path $src)) {
        Write-Warning "Source script not found: $src. Skipping."
        continue
    }
    $exeName = [IO.Path]::GetFileNameWithoutExtension($s) + '.exe'
    $out = Join-Path $binDir $exeName
    try {
        Convert-ScriptToExe -scriptPath $src -outExe $out
        Write-Host "Created: $out" -ForegroundColor Green
    }
    catch {
        Write-Warning ("Failed to convert {0}: {1}" -f $s, $_.Exception.Message)
    }
}

Write-Host 'Done. EXE files (if created) are in the /bin folder.' -ForegroundColor Cyan
