MT6835 SPI Timing / Logic‑Check

Zweck
- Kurzer Test, um SCK/MISO/MOSI-Verhalten bei 16 MHz zu prüfen.

Vorbereitung
- `encoder_mt6835.c` verwendet nun `MT6835_SPI_HZ = 16000000u`.
- `tools/mt6835_spi_check.c` ist ein kleines RP2040-C-Programm, das Burst-Reads durchführt.

Anleitung
1. Füge `tools/mt6835_spi_check.c` temporär zum CMake-Target hinzu (z.B. als `spi_check`), oder kompiliere es separat mit pico-sdk.
2. Flash das Binärfile auf das Board (oder starte via GDB/Debugger).
3. Schließe einen Logic-Analyzer an SCK, MISO, MOSI und CSN an.
4. Starte das Programm. Beobachte auf dem Analyzer:
   - SCK Takt: ~16 MHz (ggf. mit Sampling-HZ >= 50 MHz messen)
   - CSN: kurze Low-Pulse für Burst-Read (~7+ bytes)
   - MISO: gültige Daten nach kommando
5. Werte:
   - Prüfe Signalform (Stellen, Jitter, Flanken) und ob CRC/Angle plausibel ausgegeben werden.

Hinweis
- Falls Signalprobleme auftreten, reduziere `MT6835_SPI_HZ` schrittweise (8 MHz, 4 MHz, 1 MHz) und wiederhole den Test.
- Achte auf korrekte Längen/Schirmung der Leitungen und gemeinsame Masse.
