$ports=[System.IO.Ports.SerialPort]::GetPortNames()
Write-Output ('Ports: ' + ($ports -join ', '))

Get-CimInstance Win32_SerialPort | Select-Object DeviceID,Caption,Status,PNPDeviceID | Format-List

$dev = Get-PnpDevice -FriendlyName '*COM4*' -ErrorAction SilentlyContinue
if ($dev) {
    Write-Output 'PnPDevice for COM4:'
    $dev | Select-Object Status,Class,InstanceId,FriendlyName | Format-List
} else {
    Write-Output 'PnPDevice for COM4 not found'
}

try {
    $sp = New-Object System.IO.Ports.SerialPort 'COM4',115200,'None',8,'One'
    $sp.ReadTimeout = 200
    $sp.Open()
    Write-Output 'Open COM4: OK'
    $sp.Close()
} catch {
    Write-Output ('Open COM4: ERROR - ' + $_.Exception.Message)
}
