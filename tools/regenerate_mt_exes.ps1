$root = '\\nas\\Daten\\SpindelController\\SpindelControllerAS5600_Teil-Hardwaretest_SPAS5600'
$scriptPath = Join-Path $root 'tools\mt6835_stream_gui.ps1'
$out1 = Join-Path $root 'bin\mt6835_stream_gui_noconsole.exe'
$out2 = Join-Path $root 'bin\mt6835_stream_gui.exe'

Import-Module ps2exe -ErrorAction Stop

Write-Output 'Compiling no-console...'
Invoke-ps2exe -inputFile $scriptPath -outputFile $out1 -noConsole -verbose

Write-Output 'Compiling console...'
Invoke-ps2exe -inputFile $scriptPath -outputFile $out2 -verbose

Write-Output 'Creating ZIP...'
if (Test-Path $out1 -and Test-Path $out2) {
    Compress-Archive -Path $out1,$out2 -DestinationPath (Join-Path $root 'bin\mt6835_stream_gui_bundle.zip') -Force
} else {
    Write-Error 'One or both EXEs missing; aborting ZIP.'
}

Get-ChildItem -Path (Join-Path $root 'bin') -Filter 'mt6835*' -File | Select-Object Name,CreationTime,LastWriteTime,Length | Format-Table -AutoSize
