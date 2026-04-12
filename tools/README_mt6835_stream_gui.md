MT6835 Stream GUI

Files:
- mt6835_stream_gui.ps1 : PowerShell WinForms GUI to connect to serial port, send start/stop stream ('S' / 's') and show incoming lines.
- build_mt6835_stream_gui_exe.ps1 : Helper script to package the PS1 into an EXE using `ps2exe` module.

Usage:
1. Run the script directly (PowerShell):
   powershell -ExecutionPolicy Bypass -File tools\mt6835_stream_gui.ps1

2. To build EXE (requires ps2exe):
   Install-Module -Name ps2exe -Scope CurrentUser
   powershell -ExecutionPolicy Bypass -File tools\build_mt6835_stream_gui_exe.ps1

Behavior:
- Default baud: 115200. Select the COM port and Connect.
- Click "Start Stream" to send 'S' to the device; click again (Stop Stream) to send 's'.
- Incoming serial lines are shown in the log window with timestamps.

Notes:
- Ensure no other program holds the serial port (VSCode serial monitor etc.).
- If packaging fails, run the packaging command manually and inspect errors.
