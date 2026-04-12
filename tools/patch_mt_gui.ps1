$path = '\\nas\\Daten\\SpindelController\\SpindelControllerAS5600_Teil-Hardwaretest_SPAS5600\\tools\\mt6835_stream_gui.ps1'
$text = Get-Content -Raw -Path $path -ErrorAction Stop

$old1 = @'
            $script:serialPort.DataReceived.Add( {
                try {
                    $line = $script:serialPort.ReadLine()
                    Append-Log $line
                } catch {
                    Append-Log ('DataReceived handler error: ' + $_.Exception.Message)
                }
            })
'@

$new1 = @'
            # Safe DataReceived handler: guard against null/disposed port and partial reads
            $script:serialPort.DataReceived.Add( {
                try {
                    $sp = $script:serialPort
                    if ($null -eq $sp) { return }
                    if (-not $sp.IsOpen) { return }
                    $line = $null
                    try { $line = $sp.ReadExisting() } catch { }
                    if ([string]::IsNullOrEmpty($line)) {
                        try { $line = $sp.ReadLine() } catch { }
                    }
                    if (-not [string]::IsNullOrEmpty($line)) { Append-Log $line }
                } catch {
                    $msg = if ($_.Exception -ne $null) { $_.Exception.Message } else { $_.ToString() }
                    Append-Log ('DataReceived handler error: ' + $msg)
                }
            })
'@

$text = $text -replace [regex]::Escape($old1), $new1

$old2 = "        if ($cbPorts.SelectedItem -eq $null) { [System.Windows.Forms.MessageBox]::Show('Kein Port ausgewaehlt') | Out-Null; return }`r`n        $portName = $cbPorts.SelectedItem.ToString()"
$new2 = "        if ($cbPorts.SelectedItem -eq $null) { [System.Windows.Forms.MessageBox]::Show('Kein Port ausgewaehlt') | Out-Null; return }`r`n        $sel = $cbPorts.SelectedItem`r`n        if ($sel -eq $null) { [System.Windows.Forms.MessageBox]::Show('Kein Port ausgewaehlt') | Out-Null; return }`r`n        $portName = $sel.ToString()"
$text = $text -replace [regex]::Escape($old2), $new2

$old3 = "        try { $script:serialPort.WriteLine('S') } catch { Append-Log 'Fehler beim Senden start' }"
$new3 = "        try { if ($script:serialPort -and $script:serialPort.IsOpen) { $script:serialPort.WriteLine('S') } else { Append-Log 'Serial port not open when sending start' } } catch { Append-Log ('Fehler beim Senden start: ' + ($_.Exception.Message -or $_.ToString())) }"
$text = $text -replace [regex]::Escape($old3), $new3

$old4 = "        try { $script:serialPort.WriteLine('s') } catch { Append-Log 'Fehler beim Senden stop' }"
$new4 = "        try { if ($script:serialPort -and $script:serialPort.IsOpen) { $script:serialPort.WriteLine('s') } else { Append-Log 'Serial port not open when sending stop' } } catch { Append-Log ('Fehler beim Senden stop: ' + ($_.Exception.Message -or $_.ToString())) }"
$text = $text -replace [regex]::Escape($old4), $new4

Set-Content -Path $path -Value $text -Encoding UTF8
Write-Output 'Patched file'
