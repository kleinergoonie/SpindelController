$root = '\\nas\\Daten\\SpindelController\\SpindelControllerAS5600_Teil-Hardwaretest_SPAS5600'
$out1 = Join-Path $root 'bin\mt6835_stream_gui_noconsole.exe'
$out2 = Join-Path $root 'bin\mt6835_stream_gui.exe'
$zip = Join-Path $root 'bin\mt6835_stream_gui_bundle.zip'

if ((Test-Path $out1) -and (Test-Path $out2)) {
    Compress-Archive -Path $out1,$out2 -DestinationPath $zip -Force
    Write-Output "ZIP created: $zip"
} else {
    Write-Error 'One or both EXEs missing; aborting ZIP.'
}

Get-ChildItem -Path (Join-Path $root 'bin') -Filter 'mt6835*' -File | Select-Object Name,CreationTime,LastWriteTime,Length | Format-Table -AutoSize
