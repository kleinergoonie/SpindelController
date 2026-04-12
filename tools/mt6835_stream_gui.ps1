using namespace System.Windows.Forms
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

[void][System.Reflection.Assembly]::LoadWithPartialName("System.IO.Ports")

$Form = New-Object System.Windows.Forms.Form
$Form.Text = "MT6835 Stream GUI"
$Form.Size = New-Object System.Drawing.Size(800,600)
$Form.StartPosition = "CenterScreen"

$topPanel = New-Object System.Windows.Forms.Panel
$topPanel.Dock = 'Top'
$topPanel.Height = 70
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

$statusLabel = New-Object System.Windows.Forms.Label
$statusLabel.Text = "Disconnected"
$statusLabel.Location = New-Object System.Drawing.Point(8,42)
$statusLabel.AutoSize = $true
$topPanel.Controls.Add($statusLabel)

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
$script:rxTimer = New-Object System.Windows.Forms.Timer
$script:rxTimer.Interval = 50

function Append-Log([string]$text) {
    if ($Form.InvokeRequired) {
        $Form.Invoke([Action[string]]{ param($msg) Append-Log $msg }, $text) | Out-Null
        return
    }
    $t = (Get-Date).ToString('HH:mm:ss') + ' ' + $text + "`r`n"
    $txtLog.AppendText($t)
    $txtLog.SelectionStart = $txtLog.Text.Length
    $txtLog.ScrollToCaret()
    try { Add-Content -LiteralPath $script:logFile -Value $t -Encoding UTF8 } catch { }
}

function Refresh-Ports {
    $cbPorts.Items.Clear()
    $ports = [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object
    foreach ($p in $ports) { $cbPorts.Items.Add($p) | Out-Null }
    Append-Log ('Available ports: ' + ($ports -join ', '))
    if ($cbPorts.Items.Count -gt 0) { $cbPorts.SelectedIndex = 0 }
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
            }
        }
    } catch {
        $msg = if ($_.Exception) { $_.Exception.Message } else { $_.ToString() }
        Append-Log ('RX timer error: ' + $msg)
        Disconnect-Port
    }
})

$btnRefresh.Add_Click({ Refresh-Ports })

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
            [System.Windows.Forms.MessageBox]::Show('Port konnte nicht geoeffnet werden: ' + $err) | Out-Null
            Append-Log ('Open failed for ' + $portName + ' @ ' + $baud + ' : ' + $err)
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
            Append-Log ('Fehler beim Senden start: ' + $msg)
        }
    } else {
        try {
            $script:serialPort.WriteLine('s')
            $script:streaming = $false
            $btnStream.Text = 'Start Stream'
            Append-Log 'Stream gestoppt (s gesendet)'
        } catch {
            $msg = if ($_.Exception) { $_.Exception.Message } else { $_.ToString() }
            Append-Log ('Fehler beim Senden stop: ' + $msg)
        }
    }
})

$Form.Add_FormClosing({
    Disconnect-Port
})

try { Add-Content -LiteralPath $script:logFile -Value ('==== ' + (Get-Date).ToString('yyyy-MM-dd HH:mm:ss') + ' GUI Start ====') -Encoding UTF8 } catch { }

Refresh-Ports
$Form.Add_Shown({ $Form.Activate() })
[void][System.Windows.Forms.Application]::Run($Form)

