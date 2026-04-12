$p='\\nas\\Daten\\SpindelController\\SpindelControllerAS5600_Teil-Hardwaretest_SPAS5600\\SpindelControllerAS5600.c'
$lines=Get-Content -LiteralPath $p
$i = [Array]::IndexOf($lines, 'static uint32_t g_last_mt6835_register_report_ms = 0u;')
if ($i -ge 0) {
    $before = $lines[0..$i]
    $after = $lines[($i+1)..($lines.Count-1)]
    $insert = @('static volatile bool g_serial_streaming = false;','static uint32_t g_last_serial_stream_ms = 0u;','static const uint32_t SERIAL_STREAM_INTERVAL_MS = 200u;')
    $new = $before + $insert + $after
    $new | Set-Content -LiteralPath $p -Encoding UTF8
    Write-Output "inserted globals at index $i"
} else {
    Write-Output 'pattern not found'
}
