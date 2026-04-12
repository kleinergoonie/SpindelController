param(
    [string]$Port = "",
    [int]$Baud = 115200
)

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# ---------------------------------------------------------------
# Hilfsfunktionen
# ---------------------------------------------------------------
function Get-Ports {
    return [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object
}

function Update-PortList {
    $selected = $cmbPort.SelectedItem
    $cmbPort.Items.Clear()
    Get-Ports | ForEach-Object { [void]$cmbPort.Items.Add($_) }
    if ($selected -and $cmbPort.Items.Contains($selected)) {
        $cmbPort.SelectedItem = $selected
    } elseif ($Port -and $cmbPort.Items.Contains($Port)) {
        $cmbPort.SelectedItem = $Port
    } elseif ($cmbPort.Items.Count -gt 0) {
        $cmbPort.SelectedIndex = 0
    }
}

function Close-Port {
    if ($script:sp) {
        try { if ($script:sp.IsOpen) { $script:sp.Close() } } catch {}
        try { $script:sp.Dispose() } catch {}
        $script:sp = $null
    }
}

function Write-Log {
    param([string]$line)
    $stamp = (Get-Date).ToString("HH:mm:ss.fff")
    $txtLog.AppendText("[$stamp] $line`r`n")
    $txtLog.SelectionStart = $txtLog.TextLength
    $txtLog.ScrollToCaret()
}

# Farb-Hilfsfunktion fuer Status-Labels
function Set-StatusLabel {
    param(
        [System.Windows.Forms.Label]$Label,
        [string]$Text,
        [string]$State   # OK | WARN | ERR | NONE
    )
    $Label.Text = $Text
    switch ($State) {
        "OK"   { $Label.BackColor = [System.Drawing.Color]::FromArgb(30,160,60);   $Label.ForeColor = [System.Drawing.Color]::White }
        "WARN" { $Label.BackColor = [System.Drawing.Color]::FromArgb(220,160,0);   $Label.ForeColor = [System.Drawing.Color]::White }
        "ERR"  { $Label.BackColor = [System.Drawing.Color]::FromArgb(200,40,40);   $Label.ForeColor = [System.Drawing.Color]::White }
        default{ $Label.BackColor = [System.Drawing.Color]::FromArgb(80,80,90);    $Label.ForeColor = [System.Drawing.Color]::White }
    }
}

# ---------------------------------------------------------------
# Zustand
# ---------------------------------------------------------------
$script:sp     = $null
$script:buf    = ""
$script:live   = @{
    raw    = "-"; deg    = "-"
    rpm    = "-"
    ovspd  = "-"; weak   = "-"; uv = "-"
    crc_ok = "-"; st     = "-"
    t_last = 0
}

# ---------------------------------------------------------------
# Formular
# ---------------------------------------------------------------
$form = New-Object System.Windows.Forms.Form
$form.Text = "MT6835 Magnet Live"
$form.Size = New-Object System.Drawing.Size(620, 520)
$form.MinimumSize = New-Object System.Drawing.Size(620, 520)
$form.StartPosition = "CenterScreen"
$form.Font = New-Object System.Drawing.Font("Segoe UI", 9)
$form.BackColor = [System.Drawing.Color]::FromArgb(30, 32, 38)
$form.ForeColor = [System.Drawing.Color]::White

# Toolbar
$pnlTop = New-Object System.Windows.Forms.Panel
$pnlTop.Dock = "Top"; $pnlTop.Height = 52
$pnlTop.BackColor = [System.Drawing.Color]::FromArgb(42, 44, 50)
$form.Controls.Add($pnlTop)

$cmbPort = New-Object System.Windows.Forms.ComboBox
$cmbPort.Location = New-Object System.Drawing.Point(10, 14)
$cmbPort.Width = 90; $cmbPort.DropDownStyle = "DropDownList"
$pnlTop.Controls.Add($cmbPort)

$btnRefresh = New-Object System.Windows.Forms.Button
$btnRefresh.Text = "↺"; $btnRefresh.Location = New-Object System.Drawing.Point(108, 13)
$btnRefresh.Width = 30; $btnRefresh.Height = 24
$pnlTop.Controls.Add($btnRefresh)

$lblBaud = New-Object System.Windows.Forms.Label
$lblBaud.Text = "Baud:"; $lblBaud.Location = New-Object System.Drawing.Point(148, 17)
$lblBaud.AutoSize = $true; $pnlTop.Controls.Add($lblBaud)

$txtBaud = New-Object System.Windows.Forms.TextBox
$txtBaud.Location = New-Object System.Drawing.Point(192, 14)
$txtBaud.Width = 70; $txtBaud.Text = $Baud.ToString()
$pnlTop.Controls.Add($txtBaud)

$btnConn = New-Object System.Windows.Forms.Button
$btnConn.Text = "Verbinden"; $btnConn.Location = New-Object System.Drawing.Point(275, 13)
$btnConn.Width = 100; $btnConn.Height = 24
$pnlTop.Controls.Add($btnConn)

$lblStatus = New-Object System.Windows.Forms.Label
$lblStatus.Text = "Nicht verbunden"
$lblStatus.Location = New-Object System.Drawing.Point(390, 17)
$lblStatus.Size = New-Object System.Drawing.Size(190, 20)
$lblStatus.TextAlign = "MiddleRight"
$lblStatus.Anchor = "Top, Right"
$lblStatus.ForeColor = [System.Drawing.Color]::FromArgb(180,180,180)
$pnlTop.Controls.Add($lblStatus)

# Magnet-Status-Panel
$pnlMag = New-Object System.Windows.Forms.GroupBox
$pnlMag.Text = "Magnet Status"
$pnlMag.Location = New-Object System.Drawing.Point(10, 62)
$pnlMag.Size = New-Object System.Drawing.Size(590, 180)
$pnlMag.Anchor = "Top, Left, Right"
$pnlMag.ForeColor = [System.Drawing.Color]::White
$form.Controls.Add($pnlMag)

function New-InfoLabel {
    param([string]$title, [int]$x, [int]$y, [int]$w = 130, [int]$h = 54)
    $cap = New-Object System.Windows.Forms.Label
    $cap.Text = $title
    $cap.Location = New-Object System.Drawing.Point($x, $y)
    $cap.Size = New-Object System.Drawing.Size($w, 16)
    $cap.Font = New-Object System.Drawing.Font("Segoe UI", 8)
    $cap.ForeColor = [System.Drawing.Color]::FromArgb(160,160,180)
    $pnlMag.Controls.Add($cap)

    $val = New-Object System.Windows.Forms.Label
    $val.Text = "-"
    $val.Location = New-Object System.Drawing.Point($x, ($y + 18))
    $val.Size = New-Object System.Drawing.Size($w, $h)
    $val.Font = New-Object System.Drawing.Font("Consolas", 18, [System.Drawing.FontStyle]::Bold)
    $val.ForeColor = [System.Drawing.Color]::White
    $val.BackColor = [System.Drawing.Color]::FromArgb(80,80,90)
    $val.TextAlign = "MiddleCenter"
    $pnlMag.Controls.Add($val)
    return [pscustomobject]@{
        Cap = $cap
        Val = $val
    }
}

$tileOvspd = New-InfoLabel "OVERSPEED"  10  22  130 54
$tileWeak  = New-InfoLabel "SCHWACH"   155  22  130 54
$tileUV    = New-InfoLabel "UNDERVOLT" 300  22  130 54
$tileCrcOk = New-InfoLabel "CRC OK"    445  22  130 54

$lblOvspd = $tileOvspd.Val
$lblWeak  = $tileWeak.Val
$lblUV    = $tileUV.Val
$lblCrcOk = $tileCrcOk.Val

$script:magTiles = @($tileOvspd, $tileWeak, $tileUV, $tileCrcOk)

function Layout-MagnetTiles {
    $margin = 10
    $gap = 10
    $count = $script:magTiles.Count
    if ($count -le 0) {
        return
    }

    $tileW = [Math]::Max(90, [int](($pnlMag.ClientSize.Width - ($margin * 2) - ($gap * ($count - 1))) / $count))
    $tileH = [Math]::Max(54, [int]($pnlMag.ClientSize.Height - 56))
    $valueY = 40
    $titleY = 22

    for ($i = 0; $i -lt $count; $i++) {
        $x = $margin + $i * ($tileW + $gap)
        $tile = $script:magTiles[$i]
        $tile.Cap.Location = New-Object System.Drawing.Point($x, $titleY)
        $tile.Cap.Size = New-Object System.Drawing.Size($tileW, 16)

        $tile.Val.Location = New-Object System.Drawing.Point($x, $valueY)
        $tile.Val.Size = New-Object System.Drawing.Size($tileW, $tileH)
    }
}

$pnlMag.Add_Resize({ Layout-MagnetTiles })

# Winkel / RAW
$pnlAngle = New-Object System.Windows.Forms.GroupBox
$pnlAngle.Text = "Winkel"
$pnlAngle.Location = New-Object System.Drawing.Point(10, 252)
$pnlAngle.Size = New-Object System.Drawing.Size(590, 110)
$pnlAngle.Anchor = "Top, Left, Right"
$pnlAngle.ForeColor = [System.Drawing.Color]::White
$form.Controls.Add($pnlAngle)

$lblDegCap = New-Object System.Windows.Forms.Label
$lblDegCap.Text = "Grad (deg)"; $lblDegCap.Location = New-Object System.Drawing.Point(10, 16)
$lblDegCap.AutoSize = $true; $lblDegCap.ForeColor = [System.Drawing.Color]::FromArgb(160,160,180)
$pnlAngle.Controls.Add($lblDegCap)

$lblDeg = New-Object System.Windows.Forms.Label
$lblDeg.Text = "-"
$lblDeg.Location = New-Object System.Drawing.Point(10, 36)
$lblDeg.Size = New-Object System.Drawing.Size(130, 62)
$lblDeg.Font = New-Object System.Drawing.Font("Consolas", 20, [System.Drawing.FontStyle]::Bold)
$lblDeg.ForeColor = [System.Drawing.Color]::White
$pnlAngle.Controls.Add($lblDeg)

$lblRpmCap = New-Object System.Windows.Forms.Label
$lblRpmCap.Text = "RPM"; $lblRpmCap.Location = New-Object System.Drawing.Point(180, 16)
$lblRpmCap.AutoSize = $true; $lblRpmCap.ForeColor = [System.Drawing.Color]::FromArgb(160,160,180)
$pnlAngle.Controls.Add($lblRpmCap)

$lblRpm = New-Object System.Windows.Forms.Label
$lblRpm.Text = "-"
$lblRpm.Location = New-Object System.Drawing.Point(155, 36)
$lblRpm.Size = New-Object System.Drawing.Size(130, 62)
$lblRpm.Font = New-Object System.Drawing.Font("Consolas", 18, [System.Drawing.FontStyle]::Bold)
$lblRpm.ForeColor = [System.Drawing.Color]::FromArgb(220,220,240)
$pnlAngle.Controls.Add($lblRpm)

$lblRawCap = New-Object System.Windows.Forms.Label
$lblRawCap.Text = "RAW"; $lblRawCap.Location = New-Object System.Drawing.Point(310, 16)
$lblRawCap.AutoSize = $true; $lblRawCap.ForeColor = [System.Drawing.Color]::FromArgb(160,160,180)
$pnlAngle.Controls.Add($lblRawCap)

$lblRaw = New-Object System.Windows.Forms.Label
$lblRaw.Text = "-"
$lblRaw.Location = New-Object System.Drawing.Point(300, 36)
$lblRaw.Size = New-Object System.Drawing.Size(130, 62)
$lblRaw.Font = New-Object System.Drawing.Font("Consolas", 18, [System.Drawing.FontStyle]::Bold)
$lblRaw.ForeColor = [System.Drawing.Color]::FromArgb(180,180,210)
$pnlAngle.Controls.Add($lblRaw)

$lblStCap = New-Object System.Windows.Forms.Label
$lblStCap.Text = "ST"; $lblStCap.Location = New-Object System.Drawing.Point(440, 16)
$lblStCap.AutoSize = $true; $lblStCap.ForeColor = [System.Drawing.Color]::FromArgb(160,160,180)
$pnlAngle.Controls.Add($lblStCap)

$lblSt = New-Object System.Windows.Forms.Label
$lblSt.Text = "-"
$lblSt.Location = New-Object System.Drawing.Point(445, 36)
$lblSt.Size = New-Object System.Drawing.Size(130, 62)
$lblSt.Font = New-Object System.Drawing.Font("Consolas", 18, [System.Drawing.FontStyle]::Bold)
$lblSt.ForeColor = [System.Drawing.Color]::FromArgb(200,200,230)
$pnlAngle.Controls.Add($lblSt)

$script:angleFields = @(
    [pscustomobject]@{ Cap = $lblDegCap; Val = $lblDeg },
    [pscustomobject]@{ Cap = $lblRpmCap; Val = $lblRpm },
    [pscustomobject]@{ Cap = $lblRawCap; Val = $lblRaw },
    [pscustomobject]@{ Cap = $lblStCap;  Val = $lblSt }
)

function Layout-AnglePanel {
    $margin = 10
    $gap = 10
    $count = $script:angleFields.Count
    if ($count -le 0) {
        return
    }

    # Panel hoeher machen, wenn das Fenster waechst.
    $targetAngleHeight = [Math]::Min(260, [Math]::Max(150, [int]($form.ClientSize.Height * 0.30)))
    $pnlAngle.Height = $targetAngleHeight

    $titleY = 18
    $valueY = 34
    $valueH = [Math]::Max(90, $pnlAngle.ClientSize.Height - 40)
    $colW = [Math]::Max(95, [int](($pnlAngle.ClientSize.Width - ($margin * 2) - ($gap * ($count - 1))) / $count))

    for ($i = 0; $i -lt $count; $i++) {
        $x = $margin + $i * ($colW + $gap)
        $field = $script:angleFields[$i]

        $field.Cap.Location = New-Object System.Drawing.Point($x, $titleY)
        $field.Val.Location = New-Object System.Drawing.Point($x, $valueY)
        $field.Val.Size = New-Object System.Drawing.Size($colW, $valueH)
    }

    # Schriftgroessen relativ zur verfuegbaren Kachelhoehe skalieren.
    $bigSizeByHeight = [Math]::Min(64, [Math]::Max(24, [int]($valueH * 0.62)))
    # Winkel-Feld zusaetzlich an Feldbreite koppeln (wegen Text "xxx.x Grad").
    $bigSizeByWidth = [Math]::Min(64, [Math]::Max(16, [int]($colW * 0.23)))
    $bigSize = [Math]::Min($bigSizeByHeight, $bigSizeByWidth)
    $midSize = [Math]::Min(44, [Math]::Max(21, [int]($valueH * 0.54)))
    $smallSize = [Math]::Min(34, [Math]::Max(19, [int]($valueH * 0.48)))

    $degSize = [Math]::Min($bigSize, [Math]::Min(52, [Math]::Max(20, [int]($valueH * 0.40))))
    $lblDeg.Font = New-Object System.Drawing.Font("Consolas", $degSize, [System.Drawing.FontStyle]::Bold)
    $lblRpm.Font = New-Object System.Drawing.Font("Consolas", $midSize, [System.Drawing.FontStyle]::Bold)
    $lblRaw.Font = New-Object System.Drawing.Font("Consolas", $midSize, [System.Drawing.FontStyle]::Bold)
    $lblSt.Font = New-Object System.Drawing.Font("Consolas", $smallSize, [System.Drawing.FontStyle]::Bold)

    # Log-Bereich unterhalb des Winkel-Panels anpassen.
    $txtLog.Top = $pnlAngle.Bottom + 10
    $availableLogH = $form.ClientSize.Height - $txtLog.Top - $statusStrip.Height - 10
    $txtLog.Height = [Math]::Max(100, $availableLogH)
}

$pnlAngle.Add_Resize({ Layout-AnglePanel })
$form.Add_Resize({ Layout-AnglePanel })

# Log
$txtLog = New-Object System.Windows.Forms.TextBox
$txtLog.Location = New-Object System.Drawing.Point(10, 342)
$txtLog.Size = New-Object System.Drawing.Size(590, 118)
$txtLog.Anchor = "Top, Bottom, Left, Right"
$txtLog.Multiline = $true; $txtLog.ScrollBars = "Vertical"; $txtLog.ReadOnly = $true
$txtLog.Font = New-Object System.Drawing.Font("Consolas", 9)
$txtLog.BackColor = [System.Drawing.Color]::FromArgb(20, 22, 28)
$txtLog.ForeColor = [System.Drawing.Color]::FromArgb(190, 220, 190)
$form.Controls.Add($txtLog)

$statusStrip = New-Object System.Windows.Forms.StatusStrip
$statusStrip.BackColor = [System.Drawing.Color]::FromArgb(42, 44, 50)
$statusItem = New-Object System.Windows.Forms.ToolStripStatusLabel
$statusItem.ForeColor = [System.Drawing.Color]::White
$statusItem.Text = "Bereit"
$statusStrip.Items.Add($statusItem) | Out-Null
$form.Controls.Add($statusStrip)

# ---------------------------------------------------------------
# Anzeige aktualisieren
# ---------------------------------------------------------------
function Update-Display {
    $d = $script:live

    # OVERSPEED
    if ($d.ovspd -eq "1") {
        Set-StatusLabel $lblOvspd "OVERSPEED!" "ERR"
    } elseif ($d.ovspd -eq "0") {
        Set-StatusLabel $lblOvspd "OK" "OK"
    } else {
        Set-StatusLabel $lblOvspd "-" "NONE"
    }

    # SCHWACH
    if ($d.weak -eq "1") {
        Set-StatusLabel $lblWeak "SCHWACH!" "WARN"
    } elseif ($d.weak -eq "0") {
        Set-StatusLabel $lblWeak "OK" "OK"
    } else {
        Set-StatusLabel $lblWeak "-" "NONE"
    }

    # UNDERVOLT
    if ($d.uv -eq "1") {
        Set-StatusLabel $lblUV "UV!" "ERR"
    } elseif ($d.uv -eq "0") {
        Set-StatusLabel $lblUV "OK" "OK"
    } else {
        Set-StatusLabel $lblUV "-" "NONE"
    }

    # CRC
    if ($d.crc_ok -eq "1") {
        Set-StatusLabel $lblCrcOk "OK" "OK"
    } elseif ($d.crc_ok -eq "0") {
        Set-StatusLabel $lblCrcOk "FEHLER!" "ERR"
    } else {
        Set-StatusLabel $lblCrcOk "-" "NONE"
    }

    $lblDeg.Text = if ($d.deg -ne "-") { "$($d.deg)`r`nGrad" } else { "-" }
    $lblRpm.Text = $d.rpm
    $lblRaw.Text = $d.raw
    $lblSt.Text  = $d.st
}

# ---------------------------------------------------------------
# Serielle Zeile parsen
# ---------------------------------------------------------------
function Parse-Line {
    param([string]$line)

    # MT6835 REG live raw=... deg=... st=... ovspd=... weak=... uv=... crc=... crc_ok=...
    if ($line -match "^MT6835 REG live") {
        if ($line -match "raw=(\d+)")       { $script:live.raw    = $Matches[1] }
        if ($line -match "deg=([\d\.]+)")   { $script:live.deg    = $Matches[1] }
        if ($line -match "st=(0x[0-9A-Fa-f]+|\d+)") { $script:live.st = $Matches[1] }
        if ($line -match "ovspd=(\d+)")     { $script:live.ovspd  = $Matches[1] }
        if ($line -match "weak=(\d+)")      { $script:live.weak   = $Matches[1] }
        if ($line -match "\buv=(\d+)")      { $script:live.uv     = $Matches[1] }
        if ($line -match "crc_ok=(\d+)")    { $script:live.crc_ok = $Matches[1] }
        $script:live.t_last = (Get-Date).Ticks
        Update-Display
        return
    }

    # ALIVE-Zeile: Backup fuer ovspd/weak/uv falls REG live fehlt
    if ($line -match "mt_ovspd=(\d+)") { $script:live.ovspd = $Matches[1] }
    if ($line -match "mt_weak=(\d+)")  { $script:live.weak  = $Matches[1] }
    if ($line -match "mt_uv=(\d+)")    { $script:live.uv    = $Matches[1] }
    if ($line -match "\brpm=([-0-9\.]+)") { $script:live.rpm = $Matches[1] }
    if ($line -match "enc_raw=(\d+)")  { $script:live.raw   = $Matches[1] }
}

# ---------------------------------------------------------------
# Timer (seriellen Port lesen)
# ---------------------------------------------------------------
$timer = New-Object System.Windows.Forms.Timer
$timer.Interval = 40

$timer.Add_Tick({
    if (-not $script:sp -or -not $script:sp.IsOpen) { return }
    try {
        $incoming = $script:sp.ReadExisting()
        if (-not [string]::IsNullOrEmpty($incoming)) {
            $script:buf += $incoming
            $parts = $script:buf -split "`r?`n"
            for ($i = 0; $i -lt ($parts.Count - 1); $i++) {
                $l = $parts[$i].Trim()
                if ($l.Length -gt 0) {
                    Parse-Line $l
                    # Nur magnet-relevante Zeilen ins Log
                    if ($l -match "MT6835 REG live|mt_ovspd|mt_weak|mt_uv|REG read failed") {
                        Write-Log $l
                    }
                }
            }
            $script:buf = $parts[$parts.Count - 1]
        }
    }
    catch {
        $timer.Stop(); Close-Port
        $btnConn.Text = "Verbinden"
        $lblStatus.Text = "Getrennt"
        $statusItem.Text = "Fehler: " + $_.Exception.Message
        Write-Log ("FEHLER: " + $_.Exception.Message)
    }
})

# ---------------------------------------------------------------
# Button-Events
# ---------------------------------------------------------------
$btnRefresh.Add_Click({ Update-PortList })

$btnConn.Add_Click({
    if ($script:sp -and $script:sp.IsOpen) {
        $timer.Stop(); Close-Port
        $btnConn.Text = "Verbinden"
        $lblStatus.Text = "Nicht verbunden"
        $statusItem.Text = "Getrennt"
        Write-Log "Verbindung geschlossen"
        return
    }

    if (-not $cmbPort.SelectedItem) {
        [System.Windows.Forms.MessageBox]::Show("Bitte COM-Port auswaehlen.") | Out-Null
        return
    }

    $b = 115200
    [int]::TryParse($txtBaud.Text, [ref]$b) | Out-Null
    $pn = [string]$cmbPort.SelectedItem

    Close-Port
    for ($i = 1; $i -le 10; $i++) {
        try {
            $script:sp = New-Object System.IO.Ports.SerialPort $pn, $b, "None", 8, "One"
            $script:sp.DtrEnable  = $true
            $script:sp.RtsEnable  = $true
            $script:sp.ReadTimeout = 50
            $script:sp.Open()
            break
        }
        catch {
            Close-Port
            Start-Sleep -Milliseconds 120
        }
    }

    if ($script:sp -and $script:sp.IsOpen) {
        $btnConn.Text = "Trennen"
        $lblStatus.Text = "Verbunden: $pn @ $b"
        $statusItem.Text = "Verbunden"
        $script:buf = ""
        Write-Log "Verbunden mit $pn @ $b"
        $timer.Start()
    } else {
        $lblStatus.Text = "Fehler beim Oeffnen"
        $statusItem.Text = "Port nicht verfuegbar"
        [System.Windows.Forms.MessageBox]::Show("Port $pn konnte nicht geoeffnet werden.") | Out-Null
    }
})

$form.Add_FormClosing({
    $timer.Stop(); Close-Port
})

# ---------------------------------------------------------------
# Start
# ---------------------------------------------------------------
Update-PortList
Layout-MagnetTiles
Layout-AnglePanel
[void]$form.ShowDialog()
