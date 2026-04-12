param(
    [switch]$HideConsole
)

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Win32Console {
    [DllImport("kernel32.dll")]
    public static extern IntPtr GetConsoleWindow();

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool SetForegroundWindow(IntPtr hWnd);
}
"@

$script:serialPort = $null
$script:buffer = ""
$script:consoleVisible = $true
$script:telemetry = @{
    board = "-"
    enc_type = "-"
    sp = "-"
    duty = "-"
    en = "-"
    mode = "-"
    brake = "-"
    fault = "-"
    st = "-"
    estop = "-"
    oc = "-"
    ot = "-"
    wd = "-"
    lat = "-"
    encf = "-"
    ia = "-"
    tq = "-"
}

function Set-ConsoleVisibility {
    param([bool]$Visible)

    $hWnd = [Win32Console]::GetConsoleWindow()
    if ($hWnd -eq [IntPtr]::Zero) {
        return $false
    }

    # SW_HIDE=0, SW_SHOW=5, SW_RESTORE=9 (Fensterzustands-Konstanten)
    if ($Visible) {
        [void][Win32Console]::ShowWindow($hWnd, 9)
        [void][Win32Console]::ShowWindow($hWnd, 5)
        [void][Win32Console]::SetForegroundWindow($hWnd)
    } else {
        [void][Win32Console]::ShowWindow($hWnd, 0)
    }

    $script:consoleVisible = [Win32Console]::IsWindowVisible($hWnd)
    return $true
}

function Start-HiddenInstance {
    if (-not $PSCommandPath) {
        return $false
    }

    try {
        $startArgs = @(
            "-NoProfile",
            "-ExecutionPolicy", "Bypass",
            "-File", $PSCommandPath
        )
        Start-Process -FilePath "powershellw.exe" -ArgumentList $startArgs | Out-Null
        return $true
    }
    catch {
        return $false
    }
}

function Update-ConsoleButtonState {
    $hWnd = [Win32Console]::GetConsoleWindow()
    if ($hWnd -eq [IntPtr]::Zero) {
        $btnConsole.Enabled = $false
        $btnConsole.Text = "No Console"
        $script:consoleVisible = $false
        return
    }

    $btnConsole.Enabled = $true
    $script:consoleVisible = [Win32Console]::IsWindowVisible($hWnd)
    if ($script:consoleVisible) {
        $btnConsole.Text = "Hide Console"
    } else {
        $btnConsole.Text = "Show Console"
    }
}

function Get-Ports {
    return [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object
}

function Set-BitLabel {
    param(
        [System.Windows.Forms.Label]$Label,
        [int]$Value
    )

    $Label.Text = $Value.ToString()
    if ($Value -eq 1) {
        $Label.BackColor = [System.Drawing.Color]::FromArgb(40, 150, 60)
        $Label.ForeColor = [System.Drawing.Color]::White
    } else {
        $Label.BackColor = [System.Drawing.Color]::FromArgb(180, 40, 40)
        $Label.ForeColor = [System.Drawing.Color]::White
    }
}

$form = New-Object System.Windows.Forms.Form
$form.Text = "AS5600 Live Monitor"
$form.Size = New-Object System.Drawing.Size(900, 620)
$form.StartPosition = "CenterScreen"
$form.Font = New-Object System.Drawing.Font("Segoe UI", 9)
$form.BackColor = [System.Drawing.Color]::FromArgb(245, 247, 250)

$topPanel = New-Object System.Windows.Forms.Panel
$topPanel.Dock = "Top"
$topPanel.Height = 90
$topPanel.BackColor = [System.Drawing.Color]::White
$form.Controls.Add($topPanel)

$lblPort = New-Object System.Windows.Forms.Label
$lblPort.Text = "Port"
$lblPort.Location = New-Object System.Drawing.Point(14, 16)
$lblPort.AutoSize = $true
$topPanel.Controls.Add($lblPort)

$cmbPort = New-Object System.Windows.Forms.ComboBox
$cmbPort.Location = New-Object System.Drawing.Point(14, 36)
$cmbPort.Width = 120
$cmbPort.DropDownStyle = "DropDownList"
$topPanel.Controls.Add($cmbPort)

$btnRefresh = New-Object System.Windows.Forms.Button
$btnRefresh.Text = "Refresh"
$btnRefresh.Location = New-Object System.Drawing.Point(145, 35)
$btnRefresh.Width = 80
$topPanel.Controls.Add($btnRefresh)

$lblBaud = New-Object System.Windows.Forms.Label
$lblBaud.Text = "Baud"
$lblBaud.Location = New-Object System.Drawing.Point(245, 16)
$lblBaud.AutoSize = $true
$topPanel.Controls.Add($lblBaud)

$txtBaud = New-Object System.Windows.Forms.TextBox
$txtBaud.Location = New-Object System.Drawing.Point(245, 36)
$txtBaud.Width = 90
$txtBaud.Text = "115200"
$topPanel.Controls.Add($txtBaud)

$btnConnect = New-Object System.Windows.Forms.Button
$btnConnect.Text = "Connect"
$btnConnect.Location = New-Object System.Drawing.Point(355, 35)
$btnConnect.Width = 100
$topPanel.Controls.Add($btnConnect)

$btnConsole = New-Object System.Windows.Forms.Button
$btnConsole.Text = "Hide Console"
$btnConsole.Location = New-Object System.Drawing.Point(470, 35)
$btnConsole.Width = 110
$topPanel.Controls.Add($btnConsole)

$groupLive = New-Object System.Windows.Forms.GroupBox
$groupLive.Text = "Live Werte"
$groupLive.Location = New-Object System.Drawing.Point(14, 100)
$groupLive.Size = New-Object System.Drawing.Size(860, 190)
$form.Controls.Add($groupLive)

$lblEncTitle = New-Object System.Windows.Forms.Label
$lblEncTitle.Text = "ENC RAW"
$lblEncTitle.Location = New-Object System.Drawing.Point(20, 30)
$lblEncTitle.AutoSize = $true
$groupLive.Controls.Add($lblEncTitle)

$lblEncValue = New-Object System.Windows.Forms.Label
$lblEncValue.Text = "-"
$lblEncValue.Font = New-Object System.Drawing.Font("Consolas", 18, [System.Drawing.FontStyle]::Bold)
$lblEncValue.Location = New-Object System.Drawing.Point(20, 52)
$lblEncValue.AutoSize = $true
$groupLive.Controls.Add($lblEncValue)

$lblRpmTitle = New-Object System.Windows.Forms.Label
$lblRpmTitle.Text = "RPM"
$lblRpmTitle.Location = New-Object System.Drawing.Point(230, 30)
$lblRpmTitle.AutoSize = $true
$groupLive.Controls.Add($lblRpmTitle)

$lblRpmValue = New-Object System.Windows.Forms.Label
$lblRpmValue.Text = "-"
$lblRpmValue.Font = New-Object System.Drawing.Font("Consolas", 18, [System.Drawing.FontStyle]::Bold)
$lblRpmValue.Location = New-Object System.Drawing.Point(230, 52)
$lblRpmValue.AutoSize = $true
$groupLive.Controls.Add($lblRpmValue)

$lblMdTitle = New-Object System.Windows.Forms.Label
$lblMdTitle.Text = "MAG"
$lblMdTitle.Location = New-Object System.Drawing.Point(430, 30)
$lblMdTitle.AutoSize = $true
$groupLive.Controls.Add($lblMdTitle)

$lblMdValue = New-Object System.Windows.Forms.Label
$lblMdValue.Text = "0"
$lblMdValue.Location = New-Object System.Drawing.Point(430, 52)
$lblMdValue.Size = New-Object System.Drawing.Size(60, 34)
$lblMdValue.TextAlign = "MiddleCenter"
$groupLive.Controls.Add($lblMdValue)

$lblMlTitle = New-Object System.Windows.Forms.Label
$lblMlTitle.Text = "SCHW"
$lblMlTitle.Location = New-Object System.Drawing.Point(510, 30)
$lblMlTitle.AutoSize = $true
$groupLive.Controls.Add($lblMlTitle)

$lblMlValue = New-Object System.Windows.Forms.Label
$lblMlValue.Text = "0"
$lblMlValue.Location = New-Object System.Drawing.Point(510, 52)
$lblMlValue.Size = New-Object System.Drawing.Size(60, 34)
$lblMlValue.TextAlign = "MiddleCenter"
$groupLive.Controls.Add($lblMlValue)

$lblMhTitle = New-Object System.Windows.Forms.Label
$lblMhTitle.Text = "STARK"
$lblMhTitle.Location = New-Object System.Drawing.Point(590, 30)
$lblMhTitle.AutoSize = $true
$groupLive.Controls.Add($lblMhTitle)

$lblMhValue = New-Object System.Windows.Forms.Label
$lblMhValue.Text = "0"
$lblMhValue.Location = New-Object System.Drawing.Point(590, 52)
$lblMhValue.Size = New-Object System.Drawing.Size(60, 34)
$lblMhValue.TextAlign = "MiddleCenter"
$groupLive.Controls.Add($lblMhValue)

Set-BitLabel -Label $lblMdValue -Value 0
Set-BitLabel -Label $lblMlValue -Value 0
Set-BitLabel -Label $lblMhValue -Value 0

# Tooltips fuer Labels (kurze deutsche Beschreibungen)
$toolTip = New-Object System.Windows.Forms.ToolTip
$toolTip.AutoPopDelay = 10000
$toolTip.InitialDelay = 500
$toolTip.ReshowDelay = 100
$toolTip.ShowAlways = $true

$toolTip.SetToolTip($lblMdTitle, "MAG: Magnet erkannt (1 = Magnet vorhanden)")
$toolTip.SetToolTip($lblMlTitle, "SCHW: Magnet zu schwach (1 = zu schwach)")
$toolTip.SetToolTip($lblMhTitle, "STARK: Magnet zu stark (1 = zu stark)")
$toolTip.SetToolTip($lblEncTitle, "Rohwert des Encoders (Raw)")
$toolTip.SetToolTip($lblRpmTitle, "Gemessene Drehzahl in U/min (RPM)")

$lblSystemTitle = New-Object System.Windows.Forms.Label
$lblSystemTitle.Text = "Projektstatus (Brake/Safety/Torque)"
$lblSystemTitle.Location = New-Object System.Drawing.Point(20, 96)
$lblSystemTitle.AutoSize = $true
$groupLive.Controls.Add($lblSystemTitle)

$lblSystemValue = New-Object System.Windows.Forms.Label
$lblSystemValue.Text = "BOARD=- ENC=- EN=- MODE=- BRAKE=- FAULT=- ST=- | E=- OC=- OT=- WD=- LAT=- ENCF=- | IA=-A TQ=-Nm SP=- DUTY=-"
$lblSystemValue.Location = New-Object System.Drawing.Point(20, 120)
$lblSystemValue.Size = New-Object System.Drawing.Size(820, 48)
$lblSystemValue.Font = New-Object System.Drawing.Font("Consolas", 10, [System.Drawing.FontStyle]::Bold)
$groupLive.Controls.Add($lblSystemValue)
# Tooltip fÃ¼r Systemanzeige (erst nach Erstellung des Labels)
$toolTip.SetToolTip($lblSystemValue, "Systemstatus: ENC=Typ, EN=Enabled, MODE=Regelmodus, weitere Flags")
$txtLog = New-Object System.Windows.Forms.TextBox
$txtLog.Location = New-Object System.Drawing.Point(14, 300)
$txtLog.Size = New-Object System.Drawing.Size(860, 245)
$txtLog.Multiline = $true
$txtLog.ScrollBars = "Vertical"
$txtLog.ReadOnly = $true
$txtLog.Font = New-Object System.Drawing.Font("Consolas", 10)
$form.Controls.Add($txtLog)

$statusStrip = New-Object System.Windows.Forms.StatusStrip
$statusLabel = New-Object System.Windows.Forms.ToolStripStatusLabel
$statusLabel.Text = "Nicht verbunden"
$statusStrip.Items.Add($statusLabel) | Out-Null
$form.Controls.Add($statusStrip)

$timer = New-Object System.Windows.Forms.Timer
$timer.Interval = 50

function Update-PortList {
    $selected = $cmbPort.SelectedItem
    $cmbPort.Items.Clear()
    Get-Ports | ForEach-Object { [void]$cmbPort.Items.Add($_) }
    if ($selected -and $cmbPort.Items.Contains($selected)) {
        $cmbPort.SelectedItem = $selected
    } elseif ($cmbPort.Items.Count -gt 0) {
        $cmbPort.SelectedIndex = 0
    }
}

function Write-LogLine {
    param([string]$line)
    $stamp = (Get-Date).ToString("HH:mm:ss.fff")
    $txtLog.AppendText("[$stamp] $line" + [Environment]::NewLine)
    $txtLog.SelectionStart = $txtLog.TextLength
    $txtLog.ScrollToCaret()
}

function Update-SystemSummary {
    $lblSystemValue.Text = ("BOARD={0} ENC={1} EN={2} MODE={3} BRAKE={4} FAULT={5} ST={6} | E={7} OC={8} OT={9} WD={10} LAT={11} ENCF={12} | IA={13}A TQ={14}Nm SP={15} DUTY={16}" -f
        $script:telemetry.board,
        $script:telemetry.enc_type,
        $script:telemetry.en,
        $script:telemetry.mode,
        $script:telemetry.brake,
        $script:telemetry.fault,
        $script:telemetry.st,
        $script:telemetry.estop,
        $script:telemetry.oc,
        $script:telemetry.ot,
        $script:telemetry.wd,
        $script:telemetry.lat,
        $script:telemetry.encf,
        $script:telemetry.ia,
        $script:telemetry.tq,
        $script:telemetry.sp,
        $script:telemetry.duty)
}

function Update-TelemetryField {
    param(
        [string]$Line,
        [string]$Name,
        [string]$Pattern
    )

    if ($Line -match $Pattern) {
        $script:telemetry[$Name] = $Matches[1]
        return $true
    }
    return $false
}

function Close-SerialConnection {
    if (-not $script:serialPort) {
        return
    }

    try {
        $isOpen = $false
        try {
            $isOpen = $script:serialPort.IsOpen
        }
        catch {
            $isOpen = $false
        }

        if ($isOpen) {
            try {
                $script:serialPort.Close()
            }
            catch {
            }
        }

        try {
            $script:serialPort.Dispose()
        }
        catch {
        }
    }
    finally {
        $script:serialPort = $null
    }
}

function ConvertFrom-TelemetryLine {
    param([string]$line)

    if ($line -match "enc_type=(MT6835|AS5600)") {
        $script:telemetry.enc_type = $Matches[1]
        if ($Matches[1] -eq "MT6835") {
            $lblMdTitle.Text = "UEBERD"
            $lblMlTitle.Text = "SCHW"
            $lblMhTitle.Text = "UV"
        } else {
            $lblMdTitle.Text = "MAG"
            $lblMlTitle.Text = "SCHW"
            $lblMhTitle.Text = "STARK"
        }
    }

    if ($line -match "enc_raw=(\d+)") {
        $lblEncValue.Text = $Matches[1]
    }
    if ($line -match "rpm=([-0-9\.]+)") {
        $lblRpmValue.Text = $Matches[1]
    }

    if ($line -match "md=(\d).*ml=(\d).*mh=(\d)") {
        Set-BitLabel -Label $lblMdValue -Value ([int]$Matches[1])
        Set-BitLabel -Label $lblMlValue -Value ([int]$Matches[2])
        Set-BitLabel -Label $lblMhValue -Value ([int]$Matches[3])
    }

    if ($line -match "mt_ovspd=(\d).*mt_weak=(\d).*mt_uv=(\d)") {
        Set-BitLabel -Label $lblMdValue -Value ([int]$Matches[1])
        Set-BitLabel -Label $lblMlValue -Value ([int]$Matches[2])
        Set-BitLabel -Label $lblMhValue -Value ([int]$Matches[3])
    }

    [void](Update-TelemetryField -Line $line -Name "enc_type" -Pattern "enc_type=([A-Za-z0-9_]+)")
    [void](Update-TelemetryField -Line $line -Name "board" -Pattern "board=([A-Za-z0-9_\-]+)")

    [void](Update-TelemetryField -Line $line -Name "sp" -Pattern "sp=([-0-9\.]+)")
    [void](Update-TelemetryField -Line $line -Name "duty" -Pattern "duty=([-0-9\.]+)")
    [void](Update-TelemetryField -Line $line -Name "en" -Pattern "en=(\d+)")
    [void](Update-TelemetryField -Line $line -Name "mode" -Pattern "mode=(\d+)")
    [void](Update-TelemetryField -Line $line -Name "brake" -Pattern "brake=(\d+)")
    [void](Update-TelemetryField -Line $line -Name "fault" -Pattern "fault=(\d+)")
    [void](Update-TelemetryField -Line $line -Name "st" -Pattern "st=(0x[0-9A-Fa-f]+)")
    [void](Update-TelemetryField -Line $line -Name "estop" -Pattern "estop=(\d+)")
    [void](Update-TelemetryField -Line $line -Name "oc" -Pattern "oc=(\d+)")
    [void](Update-TelemetryField -Line $line -Name "ot" -Pattern "ot=(\d+)")
    [void](Update-TelemetryField -Line $line -Name "wd" -Pattern "wd=(\d+)")
    [void](Update-TelemetryField -Line $line -Name "lat" -Pattern "lat=(\d+)")
    [void](Update-TelemetryField -Line $line -Name "encf" -Pattern "encf=(\d+)")
    [void](Update-TelemetryField -Line $line -Name "ia" -Pattern "ia=([-0-9\.]+)")
    [void](Update-TelemetryField -Line $line -Name "tq" -Pattern "tq=([-0-9\.]+)")
    Update-SystemSummary

    if ($line -match "ENC angle raw=(\d+)") {
        $lblEncValue.Text = $Matches[1]
    }

    Write-LogLine $line
}

$btnRefresh.Add_Click({
    Update-PortList
})

$btnConsole.Add_Click({
    if (-not $btnConsole.Enabled) {
        return
    }

    if ($script:consoleVisible) {
        $hideOk = Set-ConsoleVisibility -Visible $false
        if ((-not $hideOk) -or $script:consoleVisible) {
            # Fallback fuer Hosts, in denen ShowWindow nicht sauber funktioniert
            # (z. B. bestimmte Terminal-Integrationen): versteckte Instanz starten.
            if (Start-HiddenInstance) {
                $form.Close()
                return
            }
            [System.Windows.Forms.MessageBox]::Show("Konsole konnte nicht versteckt werden.") | Out-Null
        }
    } else {
        if (-not (Set-ConsoleVisibility -Visible $true)) {
            [System.Windows.Forms.MessageBox]::Show("Konsole konnte nicht eingeblendet werden.") | Out-Null
        }
    }

    Update-ConsoleButtonState
})

$btnConnect.Add_Click({
    if ($script:serialPort -and $script:serialPort.IsOpen) {
        $timer.Stop()
        Close-SerialConnection
        $btnConnect.Text = "Connect"
        $statusLabel.Text = "Nicht verbunden"
        Write-LogLine "Verbindung geschlossen"
        return
    }

    if (-not $cmbPort.SelectedItem) {
        [System.Windows.Forms.MessageBox]::Show("Bitte COM-Port auswaehlen.") | Out-Null
        return
    }

    $baud = 115200
    if (-not [int]::TryParse($txtBaud.Text, [ref]$baud)) {
        [System.Windows.Forms.MessageBox]::Show("Ungueltige Baudrate.") | Out-Null
        return
    }

    $portName = [string]$cmbPort.SelectedItem
    $maxAttempts = 15
    $opened = $false
    $lastError = ""

    Close-SerialConnection

    for ($i = 1; $i -le $maxAttempts; $i++) {
        try {
            $script:serialPort = New-Object System.IO.Ports.SerialPort $portName, $baud, "None", 8, "One"
            $script:serialPort.DtrEnable = $true
            $script:serialPort.RtsEnable = $true
            $script:serialPort.ReadTimeout = 50
            $script:serialPort.NewLine = "`n"
            $script:serialPort.Open()

            $opened = $true
            break
        }
        catch {
            $lastError = $_.Exception.Message
            Close-SerialConnection
            Start-Sleep -Milliseconds 150
        }
    }

    if ($opened) {
        $btnConnect.Text = "Disconnect"
        $statusLabel.Text = "Verbunden: $portName @ $baud"
        $script:buffer = ""
        Write-LogLine "Verbindung aufgebaut"
        $timer.Start()
    } else {
        $statusLabel.Text = "Nicht verbunden"
        $msg = "Port konnte nicht geoeffnet werden: " + $lastError + "`n`nHinweis: COM5 ist meist von einem anderen Programm belegt (z. B. VS Code Serial Monitor). Schliesse den anderen Monitor und versuche es erneut."
        [System.Windows.Forms.MessageBox]::Show($msg) | Out-Null
    }
})

$timer.Add_Tick({
    if (-not $script:serialPort -or -not $script:serialPort.IsOpen) {
        return
    }

    try {
        $incoming = $script:serialPort.ReadExisting()
        if ([string]::IsNullOrEmpty($incoming)) {
            return
        }

        $script:buffer += $incoming
        $parts = $script:buffer -split "`r?`n"

        for ($i = 0; $i -lt ($parts.Count - 1); $i++) {
            $line = $parts[$i].Trim()
            if ($line.Length -gt 0) {
                ConvertFrom-TelemetryLine $line
            }
        }

        $script:buffer = $parts[$parts.Count - 1]
    }
    catch {
        $timer.Stop()
        Close-SerialConnection
        $btnConnect.Text = "Connect"
        $statusLabel.Text = "Fehler: " + $_.Exception.Message
        Write-LogLine "Lesefehler: " + $_.Exception.Message
    }
})

$form.Add_FormClosing({
    $timer.Stop()
    Close-SerialConnection
})

Update-PortList
Update-SystemSummary
Update-ConsoleButtonState

if ($HideConsole) {
    $hideOk = Set-ConsoleVisibility -Visible $false
    if ((-not $hideOk) -or $script:consoleVisible) {
        if (Start-HiddenInstance) {
            return
        }
    }
    Update-ConsoleButtonState
}

[void]$form.ShowDialog()

