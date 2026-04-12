LinuxCNC -> Remora Zuordnung (minimal)

Zweck
- Minimale M-Code/G-Code-Zuordnung zur Umschaltung des Spindel-Regelmodus am Remora-basierten Spindelcontroller.

Remora-Kommandoframe
- Byte-Offsets (Little Endian):
  - [4..7]  float speed_setpoint_rpm
  - [8]     uint8_t direction (0=CW,1=CCW)
  - [9]     uint8_t brake_cmd (0=Freilauf,1=Bremsen)
  - [10]    uint8_t enable (0/1)
  - [11]    uint8_t control_mode (0=Drehzahl,1=Position)
  - [12..15] float position_setpoint_deg

Empfohlene LinuxCNC-Zuordnung
- M3 S<rpm> -> speed_setpoint_rpm=S senden, control_mode=0
- M5       -> enable=0 senden (Stopp)
- M19 P<deg>
  - Fuer Spindelorientierung verwenden.
  - control_mode=1 und position_setpoint_deg=P senden (bei Bedarf mit enable=1).
  - Umsetzung: kurz in den Positionsmodus wechseln und auf Zielposition warten; danach optional in den vorherigen Modus zurueck.

Gewindeschneiden
- Empfohlenes Host-Verhalten:
  - Bei hohen Drehzahlen im Drehzahlmodus bleiben.
  - Fuer Rigid Tapping/Orientierung nur bei niedriger Drehzahl (< POSITION_ENABLE_RPM_THRESHOLD) in den Positionsmodus wechseln.
  - Die Firmware erzwingt aus Sicherheitsgruenden automatisch den Drehzahlmodus bei Drehzahl >= POSITION_DISABLE_RPM_THRESHOLD.

Hinweise
- Die Firmware stellt `POSITION_DISABLE_RPM_THRESHOLD` und `POSITION_ENABLE_RPM_THRESHOLD` in `config.h` bereit. Host-Skripte sollten Positionsmodus nur unterhalb der Enable-Schwelle anfordern.
- Das Remora-Feedback liefert `status_bits` mit `STATUS_BIT_POSITION_DISABLED` (PDIS), damit der Host erkennt, wenn Position aktuell durch RPM unterdrueckt wird.
