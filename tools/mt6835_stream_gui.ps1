using namespace System.Windows.Forms
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Win32Console {
    [DllImport("kernel32.dll")] public static extern IntPtr GetConsoleWindow();
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
}
"@

[void][System.Reflection.Assembly]::LoadWithPartialName("System.IO.Ports")

$Form = New-Object System.Windows.Forms.Form
$Form.Text = "MT6835 Stream GUI"
$Form.Size = New-Object System.Drawing.Size(800,600)
$Form.StartPosition = "CenterScreen"

$topPanel = New-Object System.Windows.Forms.Panel
$topPanel.Dock = 'Top'
$topPanel.Height = 72
$Form.Controls.Add($topPanel)

$lblPort = New-Object System.Windows.Forms.Label
$lblPort.Text = "Port:"
$lblPort.Location = New-Object System.Drawing.Point(8,16)
$lblPort.AutoSize = $true
$topPanel.Controls.Add($lblPort)

$cbPorts = New-Object System.Windows.Forms.ComboBox
$cbPorts.Location = New-Object System.Drawing.Point(56,12)
$cbPorts.Width = 140
$cbPorts.DropDownStyle = 'DropDownList'
$topPanel.Controls.Add($cbPorts)

$btnRefresh = New-Object System.Windows.Forms.Button
$btnRefresh.Text = "Refresh"
$btnRefresh.Location = New-Object System.Drawing.Point(204,10)
$btnRefresh.Width = 70
$topPanel.Controls.Add($btnRefresh)

$lblBaud = New-Object System.Windows.Forms.Label
$lblBaud.Text = "Baud:"
$lblBaud.Location = New-Object System.Drawing.Point(290,16)
$lblBaud.AutoSize = $true
$topPanel.Controls.Add($lblBaud)

$txtBaud = New-Object System.Windows.Forms.TextBox
$txtBaud.Location = New-Object System.Drawing.Point(340,12)
$txtBaud.Width = 90
$txtBaud.Text = "115200"
$topPanel.Controls.Add($txtBaud)

$btnConnect = New-Object System.Windows.Forms.Button
$btnConnect.Text = "Connect"
$btnConnect.Location = New-Object System.Drawing.Point(450,10)
$btnConnect.Width = 90
$topPanel.Controls.Add($btnConnect)

$btnStream = New-Object System.Windows.Forms.Button
$btnStream.Text = "Start Stream"
$btnStream.Location = New-Object System.Drawing.Point(550,10)
$btnStream.Width = 100
$btnStream.Enabled = $false
$topPanel.Controls.Add($btnStream)

$btnConsole = New-Object System.Windows.Forms.Button
$btnConsole.Text = "Console"
$btnConsole.Location = New-Object System.Drawing.Point(658,10)
$btnConsole.Width = 110
$topPanel.Controls.Add($btnConsole)

$statusLabel = New-Object System.Windows.Forms.Label
$statusLabel.Text = "Disconnected"
$statusLabel.Location = New-Object System.Drawing.Point(8,42)
$statusLabel.AutoSize = $true
$topPanel.Controls.Add($statusLabel)

$fieldsPanel = New-Object System.Windows.Forms.Panel
$fieldsPanel.Dock = 'Top'
$fieldsPanel.Height = 84
$Form.Controls.Add($fieldsPanel)

function New-ValueField([string]$labelText, [int]$x, [int]$y, [int]$w=90) {
    $lbl = New-Object System.Windows.Forms.Label
    $lbl.Text = $labelText
    $xi = if ($x -is [System.Array]) { $x[0] } else { $x }
    $yi = if ($y -is [System.Array]) { $y[0] } else { $y }
    $lbl.Location = New-Object System.Drawing.Point($xi, $yi)
    $lbl.AutoSize = $true
    [void]$fieldsPanel.Controls.Add($lbl)

    $tb = New-Object System.Windows.Forms.TextBox
    $tb.Location = New-Object System.Drawing.Point($xi + 52, $yi - 3)
    $tb.Width = $w
    $tb.ReadOnly = $true
    [void]$fieldsPanel.Controls.Add($tb)
    return $tb
}

$tbRaw   = New-ValueField "RAW:" 8 10 120
$tbDeg   = New-ValueField "DEG:" 210 10 90
$tbSt    = New-ValueField "ST:" 355 10 70
$tbCrcOk = New-ValueField "CRC_OK:" 485 10 55

$tbOvspd = New-ValueField "OVSPD:" 8 42 60
$tbWeak  = New-ValueField "WEAK:" 145 42 60
$tbUv    = New-ValueField "UV:" 275 42 60
$tbCrc   = New-ValueField "CRC:" 355 42 80
$tbMode  = New-ValueField "MODE:" 530 42 85

$txtLog = New-Object System.Windows.Forms.RichTextBox
$txtLog.Dock = 'Fill'
$txtLog.ReadOnly = $true
$txtLog.Font = New-Object System.Drawing.Font('Consolas',10)
$Form.Controls.Add($txtLog)

$script:baseDir = $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($script:baseDir)) { $script:baseDir = [System.AppDomain]::CurrentDomain.BaseDirectory }
if ([string]::IsNullOrWhiteSpace($script:baseDir)) { $script:baseDir = [System.IO.Path]::GetTempPath() }
$script:logFile = Join-Path -Path $script:baseDir -ChildPath 'mt6835_stream_gui.log'
$script:serialPort = $null
$script:streaming = $false
$script:rxBuffer = ''
$script:consoleVisible = $true
$script:rxTimer = New-Object System.Windows.Forms.Timer
$script:rxTimer.Interval = 50

function Append-Log($text) {
    if ($null -eq $text) { $text = '' }
    if ($text -is [System.Array]) {
        $text = ($text | ForEach-Object { [string]$_ }) -join ', '
    } else {
        $text = [string]$text
    }

    if ($Form.InvokeRequired) {
        $Form.Invoke([Action[string]]{ param($msg) Append-Log $msg }, $text) | Out-Null
        return
    }
    $t = "$(Get-Date -Format 'HH:mm:ss') $text`r`n"
    $txtLog.AppendText($t)
    $txtLog.SelectionStart = $txtLog.Text.Length
    $txtLog.ScrollToCaret()
    try { Add-Content -LiteralPath $script:logFile -Value $t -Encoding UTF8 } catch { }
}

function Refresh-Ports {
    $cbPorts.Items.Clear()
    $ports = [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object
    foreach ($p in $ports) { $cbPorts.Items.Add($p) | Out-Null }
    Append-Log ("Available ports: $($ports -join ', ')")
    if ($cbPorts.Items.Count -gt 0) { $cbPorts.SelectedIndex = 0 }
}

function Set-ConsoleVisibility([bool]$visible) {
    $hWnd = [Win32Console]::GetConsoleWindow()
    if ($hWnd -eq [IntPtr]::Zero) {
        $script:consoleVisible = $false
        return $false
    }
    if ($visible) {
        [void][Win32Console]::ShowWindow($hWnd, 9)
        [void][Win32Console]::ShowWindow($hWnd, 5)
        [void][Win32Console]::SetForegroundWindow($hWnd)
    } else {
        [void][Win32Console]::ShowWindow($hWnd, 0)
    }
    $script:consoleVisible = [Win32Console]::IsWindowVisible($hWnd)
    return $true
}

function Update-ConsoleButtonState {
    $hWnd = [Win32Console]::GetConsoleWindow()
    if ($hWnd -eq [IntPtr]::Zero) {
        $btnConsole.Enabled = $false
        $btnConsole.Text = "No Console"
        $script:consoleVisible = $false
        return
    }

    $script:consoleVisible = [Win32Console]::IsWindowVisible($hWnd)
    $btnConsole.Enabled = $true
    if ($script:consoleVisible) {
        $btnConsole.Text = "Console: Hide"
    } else {
        $btnConsole.Text = "Console: Show"
    }
}

function Update-Mt6835Fields([string]$line) {
    if ($Form.InvokeRequired) {
        $Form.Invoke([Action[string]]{ param($l) Update-Mt6835Fields $l }, $line) | Out-Null
        return
    }

    if ([string]::IsNullOrWhiteSpace($line)) { return }

    if ($line -match 'MT6835\s+REG\s+live\s+raw=(\d+)\s+deg=([-0-9\.]+)\s+st=(0x[0-9A-Fa-f]+)\s+ovspd=(\d+)\s+weak=(\d+)\s+uv=(\d+)\s+crc=(0x[0-9A-Fa-f]+)\s+crc_ok=(\d+)') {
        $tbRaw.Text = $Matches[1]
        $tbDeg.Text = $Matches[2]
        $tbSt.Text = $Matches[3]
        $tbOvspd.Text = $Matches[4]
        $tbWeak.Text = $Matches[5]
        $tbUv.Text = $Matches[6]
        $tbCrc.Text = $Matches[7]
        $tbCrcOk.Text = $Matches[8]
        return
    }

    if ($line -match 'SERIAL\s+STREAM\s+START') {
        $tbMode.Text = 'STREAM'
        return
    }
    if ($line -match 'SERIAL\s+STREAM\s+STOP') {
        $tbMode.Text = 'IDLE'
        return
    }
    if ($line -match 'SERIAL\s+SNAPSHOT') {
        $tbMode.Text = 'SNAP'
        return
    }
}

function Disconnect-Port {
    $script:rxTimer.Stop()
    $script:rxBuffer = ''
    if ($script:serialPort) {
        try {
            if ($script:serialPort.IsOpen) { $script:serialPort.Close() }
        } catch { }
        try { $script:serialPort.Dispose() } catch { }
        $script:serialPort = $null
    }
    $statusLabel.Text = 'Disconnected'
    $btnConnect.Text = 'Connect'
    $btnStream.Enabled = $false
    $btnStream.Text = 'Start Stream'
    $script:streaming = $false
}

$script:rxTimer.Add_Tick({
    if ($null -eq $script:serialPort) { return }
    if (-not $script:serialPort.IsOpen) { return }

    try {
        $chunk = $script:serialPort.ReadExisting()
        if ([string]::IsNullOrEmpty($chunk)) { return }

        $script:rxBuffer += $chunk
        $script:rxBuffer = $script:rxBuffer -replace "`r", ''

        while ($script:rxBuffer.Contains("`n")) {
            $newlineIndex = $script:rxBuffer.IndexOf("`n")
            $line = $script:rxBuffer.Substring(0, $newlineIndex).Trim()
            if ($newlineIndex + 1 -le $script:rxBuffer.Length - 1) {
                $script:rxBuffer = $script:rxBuffer.Substring($newlineIndex + 1)
            } else {
                $script:rxBuffer = ''
            }
            if (-not [string]::IsNullOrWhiteSpace($line)) {
                Append-Log $line
                Update-Mt6835Fields $line
            }
        }
    } catch {
        $msg = if ($_.Exception) { $_.Exception.Message } else { $_.ToString() }
        Append-Log ("RX timer error: $msg")
        Disconnect-Port
    }
})

$btnRefresh.Add_Click({ Refresh-Ports })

$btnConsole.Add_Click({
    if ($btnConsole.Enabled -eq $false) { return }
    $targetVisible = -not $script:consoleVisible
    [void](Set-ConsoleVisibility $targetVisible)
    Update-ConsoleButtonState
})

$btnConnect.Add_Click({
    if (-not $script:serialPort) {
        if ($cbPorts.Items.Count -eq 0) { Refresh-Ports }
        if ($cbPorts.SelectedItem -eq $null -and $cbPorts.Items.Count -gt 0) { $cbPorts.SelectedIndex = 0 }
        if ($cbPorts.SelectedItem -eq $null) {
            [System.Windows.Forms.MessageBox]::Show('Kein Port ausgewaehlt') | Out-Null
            return
        }

        $portName = [string]$cbPorts.SelectedItem
        $baud = 115200
        if (-not [int]::TryParse($txtBaud.Text, [ref]$baud)) {
            [System.Windows.Forms.MessageBox]::Show('Ungueltige Baudrate') | Out-Null
            return
        }

        try {
            $script:serialPort = New-Object System.IO.Ports.SerialPort $portName, $baud, 'None', 8, 'One'
            $script:serialPort.DtrEnable = $true
            $script:serialPort.RtsEnable = $true
            $script:serialPort.NewLine = "`n"
            $script:serialPort.ReadTimeout = 50
            $script:serialPort.Open()
            $script:rxBuffer = ''
            $script:rxTimer.Start()

            $statusLabel.Text = "Connected: $portName @ $baud"
            $btnConnect.Text = 'Disconnect'
            $btnStream.Enabled = $true
            Append-Log "Connected to $portName @ $baud"
        } catch {
            $err = if ($_.Exception) { $_.Exception.Message } else { $_.ToString() }
            [System.Windows.Forms.MessageBox]::Show("Port konnte nicht geoeffnet werden: $err") | Out-Null
            Append-Log ("Open failed for $portName @ $baud : $err")
            Disconnect-Port
        }
    } else {
        Disconnect-Port
        Append-Log 'Disconnected'
    }
})

$btnStream.Add_Click({
    if (-not $script:serialPort -or -not $script:serialPort.IsOpen) {
        [System.Windows.Forms.MessageBox]::Show('Nicht verbunden') | Out-Null
        return
    }

    if (-not $script:streaming) {
        try {
            $script:serialPort.WriteLine('S')
            $script:streaming = $true
            $btnStream.Text = 'Stop Stream'
            Append-Log 'Stream gestartet (S gesendet)'
            } catch {
            $msg = if ($_.Exception) { $_.Exception.Message } else { $_.ToString() }
            Append-Log ("Fehler beim Senden start: $msg")
        }
    } else {
        try {
            $script:serialPort.WriteLine('s')
            $script:streaming = $false
            $btnStream.Text = 'Start Stream'
            Append-Log 'Stream gestoppt (s gesendet)'
        } catch {
            $msg = if ($_.Exception) { $_.Exception.Message } else { $_.ToString() }
            Append-Log ("Fehler beim Senden stop: $msg")
        }
    }
})

$Form.Add_FormClosing({
    Disconnect-Port
})

try { Add-Content -LiteralPath $script:logFile -Value ("==== $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') GUI Start ====") -Encoding UTF8 } catch { }

Refresh-Ports
Update-ConsoleButtonState
$Form.Add_Shown({ $Form.Activate() })
[void][System.Windows.Forms.Application]::Run($Form)

