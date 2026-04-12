$p='\\nas\\Daten\\SpindelController\\SpindelControllerAS5600_Teil-Hardwaretest_SPAS5600\\SpindelControllerAS5600.c'
$txt = Get-Content -Raw -LiteralPath $p
$old = 'mt6835_register_report_if_due(now_ms);'
$insert = @'
mt6835_register_report_if_due(now_ms);
    /* Serial command polling (non-blocking): S=start stream, s=stop, R=snapshot */
    {
        int c = getchar_timeout_us(0);
        if (c != PICO_ERROR_TIMEOUT) {
            if (c == 'S') { g_serial_streaming = true; printf("STREAM START\n"); }
            else if (c == 's') { g_serial_streaming = false; printf("STREAM STOP\n"); }
            else if (c == 'R') {
#if (MODULE_ENCODER_ENABLE && (ENCODER_TYPE_SELECT == ENCODER_TYPE_MT6835))
                mt6835_report_registers();
#endif
            }
        }
    }
    if (g_serial_streaming) {
        if ((now_ms - g_last_serial_stream_ms) >= SERIAL_STREAM_INTERVAL_MS) {
#if (MODULE_ENCODER_ENABLE && (ENCODER_TYPE_SELECT == ENCODER_TYPE_MT6835))
            mt6835_report_registers();
#endif
            g_last_serial_stream_ms = now_ms;
        }
    }
'@
if ($txt -like "*${old}*") {
    $new = $txt -replace [regex]::Escape($old), $insert, 1
    Set-Content -LiteralPath $p -Value $new -Encoding UTF8
    Write-Output 'inserted streaming block'
} else {
    Write-Output 'pattern not found'
}
