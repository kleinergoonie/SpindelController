#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

// ============================================================
// 1. BUILD- UND MODULSCHALTER
// ============================================================

// Globale Modulschalter (1=aktiv, 0=inaktiv).
#define MODULE_W5500_ENABLE   1u
#define MODULE_ENCODER_ENABLE 1u
#define MODULE_DISPLAY_ENABLE 0u
#define MODULE_TOUCH_ENABLE   0u
#define MODULE_ADC_ENABLE     0u
#define MODULE_WS2812_ENABLE  1u

// Globaler Toleranzmodus fuer Peripherie (1=fehlende externe Peripherie ignorieren).
#define HARDWARETEST_MODE            0u
#define HARDWARETEST_MINIMAL_DIAGNOSTIC 0u
// Schrittweises Hochfahren im Hardwaretest-Modus:
// 0=nur Board-LED + WS2812, 1=+Motor+Safety, 2=+ADC,
// 3=+Remora/W5500, 4=+UI-Core/Display/Touch, 5=+Encoder+Regeltimer (voller Pfad).
#define HARDWARETEST_BRINGUP_STAGE   5u

// Hardware watchdog for runtime safety (1=enabled in production runs).
#define HARDWARE_WATCHDOG_ENABLE 1u

// ============================================================
// 2. PIN-BELEGUNG
// ============================================================

// UART.
#define PIN_UART_TX       0u
#define PIN_UART_RX       1u

// Safety-Eingang.
#define PIN_ESTOP         3u

// MT6835-Absolutencoder auf SPI1.
#define PIN_MT6835_MISO   4u
#define PIN_MT6835_MOSI   5u
#define PIN_MT6835_SCK    6u
#define PIN_MT6835_CSN    7u
#define MT6835_SPI_IF     spi1

// AS5600-Magnetencoder auf I2C0.
// (GP4/GP5 sind mit MT6835 geteilt - nur ein Encodertyp ist gleichzeitig aktiv.)
#define PIN_AS5600_SDA    4u
#define PIN_AS5600_SCL    5u

// Optionaler Touch-Controller auf gemeinsamem SPI0 (XPT2046-kompatibel).
#define PIN_TOUCH_CS      8u
#define PIN_TOUCH_IRQ     11u

// ST7789-Display auf gemeinsamem SPI0.
#define PIN_ST7789_CS     9u
#define PIN_ST7789_BL     10u
#define PIN_ST7789_DC     21u
#define PIN_ST7789_RST    22u

// H-Bridge-Motortreiber.
#define PIN_MOTOR_PWM_A   12u
#define PIN_MOTOR_PWM_B   13u
#define PIN_MOTOR_BRAKE   14u
#define PIN_MOTOR_ENABLE  15u

// W5500-Ethernetcontroller auf SPI0.
#define PIN_W5500_MISO    16u
#define PIN_W5500_CS      17u
#define PIN_W5500_SCK     18u
#define PIN_W5500_MOSI    19u
#define W5500_RST_PIN     20u
#define W5500_SPI_IF      spi0
#define W5500_SPI_HZ      62500000u

// Optionale ADC-Kanaele.
#define PIN_ADC_CURRENT   26u
#define PIN_ADC_TEMP      27u

// Board-Steuerpins -- haengt vom gewaehlten PICO_BOARD ab.
// Das Makro BOARD_WAVESHARE_RP2040_ZERO bzw. BOARD_WAVESHARE_RP2040_PLUS_16MB
// wird per CMake target_compile_definitions gesetzt (siehe CMakeLists.txt).
#if defined(BOARD_WAVESHARE_RP2040_ZERO)
// Waveshare RP2040 Zero: kein separater GPIO-LED, kein User-Key.
// Onboard-LED ist ein NeoPixel (WS2812) auf GP16.
#define BOARD_HAS_GPIO_LED  0u
#define BOARD_HAS_USER_KEY  0u
#define BOARD_NAME_TEXT     "waveshare_rp2040_zero"
#define PIN_BOARD_LED       16u   // WS2812-Pin (PWM-board_led deaktiviert)
#define PIN_BOARD_KEY       255u  // nicht vorhanden
#define PIN_WS2812_DATA     16u   // onboard NeoPixel

// Zero-Board: MT6835 auf spi0 ueber ausgefuehrte Pins.
#undef PIN_MT6835_MISO
#undef PIN_MT6835_MOSI
#undef PIN_MT6835_SCK
#undef PIN_MT6835_CSN
#undef MT6835_SPI_IF
#define PIN_MT6835_MISO    4u
#define PIN_MT6835_MOSI    7u
#define PIN_MT6835_SCK     6u
#define PIN_MT6835_CSN     5u
#define MT6835_SPI_IF      spi0

// Zero-Board: separate W5500-Belegung ueber ausgefuehrte Pins (keine reinen Pads),
// damit GP16 exklusiv fuer die Onboard-WS2812 frei bleibt.
#undef PIN_W5500_MISO
#undef PIN_W5500_CS
#undef PIN_W5500_SCK
#undef PIN_W5500_MOSI
#undef W5500_RST_PIN
#undef W5500_SPI_IF
#define PIN_W5500_MISO     8u
#define PIN_W5500_CS       13u
#define PIN_W5500_SCK      10u
#define PIN_W5500_MOSI     11u
#define W5500_RST_PIN      9u
#define W5500_SPI_IF       spi1
#elif defined(BOARD_WAVESHARE_RP2040_PLUS_16MB)
// Waveshare RP2040 Plus 16MB: GPIO-LED GP25, User-Key GP24, ext. WS2812 GP23.
#define BOARD_HAS_GPIO_LED  1u
#define BOARD_HAS_USER_KEY  1u
#define BOARD_NAME_TEXT     "waveshare_rp2040_plus_16mb"
#define PIN_BOARD_LED       25u
#define PIN_BOARD_KEY       24u
#define PIN_WS2812_DATA     23u
#else
#error "Unbekanntes Board - bitte board-spezifischen Abschnitt in config.h erweitern"
#endif

// Optionaler Guard: erzwingt bei absichtlicher Fehlbelegung (WS2812==W5500 MISO)
// weiterhin das Abschalten des W5500-Moduls auf dem Zero.
#if defined(BOARD_WAVESHARE_RP2040_ZERO)
#ifndef ZERO_BOARD_ALLOW_W5500_ON_WS2812_PIN
#define ZERO_BOARD_ALLOW_W5500_ON_WS2812_PIN 0u
#endif
#if MODULE_WS2812_ENABLE && MODULE_W5500_ENABLE && (PIN_WS2812_DATA == PIN_W5500_MISO) && (ZERO_BOARD_ALLOW_W5500_ON_WS2812_PIN == 0u)
#undef MODULE_W5500_ENABLE
#define MODULE_W5500_ENABLE 0u
#endif
#endif

// Spindel-Steuerausgang (PWM, analog-aehnlich). Einen freien GPIO waehlen.
#define PIN_SPINDLE_CTRL  2u

// Spindel-Sollwertabbildung: 0..SPINDLE_CTRL_VMAX_V entspricht 0..100% Duty
#define SPINDLE_CTRL_VMAX_V 5.0f
#define SPINDLE_CTRL_FREQ_HZ 1000u

// ============================================================
// 3. TIMING- UND LOOP-RATEN
// ============================================================

// PWM-Traegerfrequenz des Motors.
#define MOTOR_PWM_FREQ_HZ  20000u

// Encoder-Abtastung, Regelschleife und UI-Aktualisierungsraten.
#define ENCODER_SAMPLE_HZ  160000u
#define CONTROL_LOOP_HZ    10000u
#define UI_UPDATE_HZ       10u
#define ENCODER_TIMEOUT_MS 100u

// Gemeinsame SPI0-Busgeschwindigkeit fuer W5500, Display und Touch-Controller.
#define SPI0_BUS_HZ        62500000u

// ============================================================
// 4. ENCODER-KONFIGURATION
// ============================================================

// Auswahl des Encodertyps.
#define ENCODER_TYPE_MT6835 1u
#define ENCODER_TYPE_AS5600 0u
#define ENCODER_TYPE_SELECT ENCODER_TYPE_MT6835

// Skalierung MT6835-Absolutencoder (21-Bit).
#define MT6835_ANGLE_MAX  2097152u // 2^21

// Konfiguration AS5600-Magnetencoder.
#define AS5600_ANGLE_MAX             4096u  // 12-bit
#define AS5600_I2C_ADDR              0x36u
#define AS5600_STATUS_POLL_INTERVAL_MS 20u
// DIR ist beim AS5600 ein Hardware-Pin (nicht per I2C setzbar).
// DIR an GND fuer im Uhrzeigersinn steigenden Winkel, an VDD fuer gegen den Uhrzeigersinn.

// AS5600-I2C-Erkennungsdetails einmalig beim Start ausgeben.
#define AS5600_STARTUP_I2C_SCAN_ENABLE 1u

// ============================================================
// 5. MOTOR- UND PID-KONFIGURATION
// ============================================================

// Drehzahlgrenzen der Spindel.
#define SPINDLE_RPM_MIN        0.0f
#define SPINDLE_RPM_MAX        12000.0f
#define SPINDLE_RAMP_RPM_PER_S 3000.0f

// Regelmodus (zur Laufzeit ueber Remora-Kommandoframe umschaltbar).
#define CONTROL_MODE_SPEED      0u
#define CONTROL_MODE_POSITION   1u
#define CONTROL_MODE_DEFAULT    CONTROL_MODE_POSITION

// Positionsregelung (Single-Turn, kuerzester Weg in Grad).
#define POSITION_KP_DEFAULT      0.01f
#define POSITION_DEADBAND_DEG    0.5f
#define POSITION_MAX_DUTY        0.8f

// Automatische Deaktivierung der Positionsregelung bei hoher Drehzahl
// (fuer Gewindeschneiden / Encoder-Synchronisationssicherheit).
// Hysterese: deaktivieren bei >= Abschaltschwelle, wieder freigeben bei < Freigabeschwelle.
#define POSITION_DISABLE_RPM_THRESHOLD  5000.0f
#define POSITION_ENABLE_RPM_THRESHOLD   4500.0f

// PID-Standardwerte gemaess abgestimmtem Plan.
#define PID_KP_DEFAULT  1.0f
#define PID_KI_DEFAULT  0.25f
#define PID_KD_DEFAULT  0.005f

// Geschaetzte Drehmomentkonstante fuer die reine Anzeige-Berechnung.
// Drehmomentschaetzung: T_est = I_motor * MOTOR_TORQUE_CONSTANT_NM_PER_A
#define MOTOR_TORQUE_CONSTANT_NM_PER_A 0.060f

// Anzeigemodus fuer Drehmoment.
#define TORQUE_DISPLAY_SIGNED 0u
#define TORQUE_DISPLAY_ABS    1u
#define TORQUE_DISPLAY_MODE   TORQUE_DISPLAY_SIGNED

// ============================================================
// 6. NETZWERK - W5500
// ============================================================

// MAC-Adresse (lokal administrierter Unicast).
#define W5500_MAC_0  0x00u
#define W5500_MAC_1  0x08u
#define W5500_MAC_2  0xDCu
#define W5500_MAC_3  0xABu
#define W5500_MAC_4  0xCDu
#define W5500_MAC_5  0xEFu

// Statische IP / Subnetz / Gateway (primaer oder DHCP-Fallback).
#define W5500_IP_0  192u
#define W5500_IP_1  168u
#define W5500_IP_2  2u
#define W5500_IP_3  125u

#define W5500_SN_0  255u
#define W5500_SN_1  255u
#define W5500_SN_2  255u
#define W5500_SN_3  0u

#define W5500_GW_0  192u
#define W5500_GW_1  168u
#define W5500_GW_2  2u
#define W5500_GW_3  1u

// W5500-Adressmodus.
// 0 = statische IP aus W5500_IP/SN/GW-Defines, 1 = DHCP mit optionalem statischen Fallback.
#define W5500_USE_DHCP             1u
#define W5500_DHCP_TIMEOUT_S       2u
#define W5500_DHCP_FALLBACK_STATIC 1u
#define W5500_PHY_LINK_WAIT_MS     1000u
#define W5500_DHCP_RETRY_TIMEOUT_S 10u

// SPI-Bursttransfer ueber DMA.
#define W5500_SPI_DMA_ENABLE          1u
#define W5500_SPI_DMA_MIN_BURST_BYTES 8u

// ============================================================
// 7. REMORA-TRANSPORT
// ============================================================

// Remora-Serveradresse und Port.
#define REMORA_SERVER_IP_0  192u
#define REMORA_SERVER_IP_1  168u
#define REMORA_SERVER_IP_2  2u
#define REMORA_SERVER_IP_3  48u
#define REMORA_SERVER_PORT  27181u

// Kommandopakete nur vom konfigurierten Remora-Host/Port akzeptieren.
#define REMORA_STRICT_HOST_FILTER 1u

// Remora/W5500-Transporttiming (kleinere Werte reduzieren LAN-Reaktionslatenz).
#define REMORA_TRANSPORT_RETRY_MS 200u
#define REMORA_POLL_SLEEP_MS      0u
#define REMORA_RX_BURST_MAX       4u

// ============================================================
// 8. DISPLAY-KONFIGURATION
// ============================================================

// Auswahl des Display-Profils.
#define DISPLAY_MODEL_ST7789       0u
#define DISPLAY_MODEL_MCUFRIEND397 1u
#define DISPLAY_MODEL_SELECT       DISPLAY_MODEL_MCUFRIEND397

// Display-Aufloesung entsprechend dem gewaehlten Panel-Profil.
#if DISPLAY_MODEL_SELECT == DISPLAY_MODEL_MCUFRIEND397
#define DISPLAY_WIDTH  480u
#define DISPLAY_HEIGHT 320u
#else
#define DISPLAY_WIDTH  280u
#define DISPLAY_HEIGHT 240u
#endif

// ============================================================
// 9. TOUCH-EINGABE-KONFIGURATION
// ============================================================

// Auswahl des Touch-Controllers.
#define TOUCH_TYPE_NONE    0u
#define TOUCH_TYPE_XPT2046 1u
#define TOUCH_TYPE_SELECT  TOUCH_TYPE_XPT2046

// Rohe XPT2046-Kalibrierwerte fuer 480x320 im Querformat.
#define TOUCH_RAW_X_MIN 220u
#define TOUCH_RAW_X_MAX 3850u
#define TOUCH_RAW_Y_MIN 260u
#define TOUCH_RAW_Y_MAX 3820u
#define TOUCH_SWAP_XY   0u
#define TOUCH_INVERT_X  0u
#define TOUCH_INVERT_Y  1u

// ============================================================
// 10. ADC UND STROMSENSOR
// ============================================================

// RP2040-ADC-Eigenschaften.
#define ADC_REF_VOLTAGE 3.3f
#define ADC_MAX_COUNT   4095.0f

// Standardwerte fuer ACS712-20A-Stromsensor.
// ACS712-Ausgang ist um VCC/2 zentriert, Empfindlichkeit 100 mV/A.
// Bei 5-V-Versorgung des Sensors ADC-Eingang <=3.3 V per Teiler sicherstellen.
// V_ADC = V_SENSOR * ACS712_ADC_DIVIDER_RATIO.
#define ACS712_SENSITIVITY_V_PER_A 0.100f
#define ACS712_ADC_DIVIDER_RATIO   0.66f
#define ACS712_ZERO_CURRENT_ADC    2048.0f
#define ACS712_AUTOZERO_SAMPLES    500u
#define ACS712_AUTOZERO_DELAY_US   200u

// Optionale Laufzeit-Neunullung im Idle-Zustand.
#define ACS712_RUNTIME_AUTOZERO_ENABLE      1u
#define ACS712_RUNTIME_AUTOZERO_INTERVAL_MS 5000u
#define ACS712_RUNTIME_AUTOZERO_SAMPLES     128u
#define ACS712_RUNTIME_AUTOZERO_DELAY_US    100u
#define ACS712_RUNTIME_AUTOZERO_MAX_ABS_A   0.5f

// ============================================================
// 11. WS2812-STATUS-LED
// ============================================================

#define WS2812_IS_RGBW             0u
#if defined(BOARD_WAVESHARE_RP2040_ZERO)
#define WS2812_BRIGHTNESS          0.2f
#elif defined(BOARD_WAVESHARE_RP2040_PLUS_16MB)
#define WS2812_BRIGHTNESS          0.005f
#else
#define WS2812_BRIGHTNESS          0.005f
#endif
#define WS2812_COLOR_ORDER_GRB     0u
#define WS2812_COLOR_ORDER_RGB     1u
#define WS2812_COLOR_ORDER         WS2812_COLOR_ORDER_GRB
#define WS2812_STARTUP_TEST_ENABLE 1u
#define WS2812_STARTUP_TEST_MS     150u
#define WS2812_FORCE_WHITE_TEST    0u

// ============================================================
// 12. DIAGNOSE UND DEBUG
// ============================================================

// Temporaerer Terminal-Filtermodus: nur W5500/Remora-Logs.
#define TERMINAL_W5500_ONLY_MODE 0u

// Heartbeat-LED aus Remora/W5500-RX-Aktivitaet.
#define HB_LED_ENABLE          0u
#define REMORA_HB_TIMEOUT_MS   1000u
#define HB_LED_BLINK_PERIOD_MS 200u
// Helligkeit der Heartbeat-LED im EIN-Zustand (0.0 .. 1.0)
#define HB_LED_BRIGHTNESS      0.05f

// Optionale Terminaldiagnose fuer Encoder-Winkelaenderungen.
#define ENCODER_TERMINAL_REPORT_ENABLE        0u
#define ENCODER_TERMINAL_REPORT_INTERVAL_MS   20u
#define ENCODER_TERMINAL_REPORT_MIN_RAW_DELTA 0u

// MT6835 Registerausgabe (sinnvolle Registerfelder auf UART).
#define MT6835_REGISTER_REPORT_ENABLE      0u
#define MT6835_REGISTER_REPORT_INTERVAL_MS 100u


// Periodischer serieller Alive-Report zur Monitor-Verbindungspruefung.
// TemporÃ¤r fÃ¼r Tests deaktiviert ("alive meldung testweise aus").
#define SERIAL_ALIVE_REPORT_ENABLE      0u
#define SERIAL_ALIVE_REPORT_INTERVAL_MS 200u

// Remora-Transportdebug ueber UART.
#define REMORA_DEBUG_ENABLE            0u
#define REMORA_DEBUG_PRINT_INTERVAL_MS 250u

// ============================================================
// 13. GEMEINSAME PROTOKOLLKONSTANTEN
// ============================================================

// Gemeinsames Statusbit-Layout fuer UI und Remora-Feedback.
#define STATUS_BIT_ESTOP         0u
#define STATUS_BIT_OVERCURRENT   1u
#define STATUS_BIT_OVERTEMP      2u
#define STATUS_BIT_WATCHDOG      3u
#define STATUS_BIT_FAULT_LATCHED 4u
#define STATUS_BIT_ENCODER_FAULT 5u
// Positionsregelung durch RPM-Hysterese deaktiviert (>= Abschaltschwelle)
#define STATUS_BIT_POSITION_DISABLED 6u

// ============================================================
// 14. COMPILE-TIME-VALIDIERUNG
// ============================================================

// Build- und Modulschalter.
#if (MODULE_W5500_ENABLE != 0u) && (MODULE_W5500_ENABLE != 1u)
#error "MODULE_W5500_ENABLE must be 0 or 1"
#endif
#if (MODULE_ENCODER_ENABLE != 0u) && (MODULE_ENCODER_ENABLE != 1u)
#error "MODULE_ENCODER_ENABLE must be 0 or 1"
#endif
#if (MODULE_DISPLAY_ENABLE != 0u) && (MODULE_DISPLAY_ENABLE != 1u)
#error "MODULE_DISPLAY_ENABLE must be 0 or 1"
#endif
#if (MODULE_TOUCH_ENABLE != 0u) && (MODULE_TOUCH_ENABLE != 1u)
#error "MODULE_TOUCH_ENABLE must be 0 or 1"
#endif
#if (MODULE_ADC_ENABLE != 0u) && (MODULE_ADC_ENABLE != 1u)
#error "MODULE_ADC_ENABLE must be 0 or 1"
#endif
#if (MODULE_WS2812_ENABLE != 0u) && (MODULE_WS2812_ENABLE != 1u)
#error "MODULE_WS2812_ENABLE must be 0 or 1"
#endif
#if (HARDWARETEST_MODE != 0u) && (HARDWARETEST_MODE != 1u)
#error "HARDWARETEST_MODE must be 0 or 1"
#endif
#if (HARDWARETEST_MINIMAL_DIAGNOSTIC != 0u) && (HARDWARETEST_MINIMAL_DIAGNOSTIC != 1u)
#error "HARDWARETEST_MINIMAL_DIAGNOSTIC must be 0 or 1"
#endif
#if (HARDWARETEST_BRINGUP_STAGE > 5u)
#error "HARDWARETEST_BRINGUP_STAGE must be in range 0..5"
#endif
#if (HARDWARE_WATCHDOG_ENABLE != 0u) && (HARDWARE_WATCHDOG_ENABLE != 1u)
#error "HARDWARE_WATCHDOG_ENABLE must be 0 or 1"
#endif

// Encoder.
#if (ENCODER_TYPE_SELECT != ENCODER_TYPE_MT6835) && (ENCODER_TYPE_SELECT != ENCODER_TYPE_AS5600)
#error "ENCODER_TYPE_SELECT must be ENCODER_TYPE_MT6835 or ENCODER_TYPE_AS5600"
#endif
#if (AS5600_STARTUP_I2C_SCAN_ENABLE != 0u) && (AS5600_STARTUP_I2C_SCAN_ENABLE != 1u)
#error "AS5600_STARTUP_I2C_SCAN_ENABLE must be 0 or 1"
#endif

// Motor.
#if (TORQUE_DISPLAY_MODE != TORQUE_DISPLAY_SIGNED) && (TORQUE_DISPLAY_MODE != TORQUE_DISPLAY_ABS)
#error "TORQUE_DISPLAY_MODE must be TORQUE_DISPLAY_SIGNED or TORQUE_DISPLAY_ABS"
#endif
#if (CONTROL_MODE_DEFAULT != CONTROL_MODE_SPEED) && (CONTROL_MODE_DEFAULT != CONTROL_MODE_POSITION)
#error "CONTROL_MODE_DEFAULT must be CONTROL_MODE_SPEED or CONTROL_MODE_POSITION"
#endif

// Netzwerk / W5500.
#if (W5500_USE_DHCP != 0u) && (W5500_USE_DHCP != 1u)
#error "W5500_USE_DHCP must be 0 or 1"
#endif
#if (W5500_DHCP_FALLBACK_STATIC != 0u) && (W5500_DHCP_FALLBACK_STATIC != 1u)
#error "W5500_DHCP_FALLBACK_STATIC must be 0 or 1"
#endif
#if (W5500_DHCP_TIMEOUT_S < 1u) || (W5500_DHCP_TIMEOUT_S > 120u)
#error "W5500_DHCP_TIMEOUT_S must be in range 1..120"
#endif
#if (W5500_PHY_LINK_WAIT_MS > 30000u)
#error "W5500_PHY_LINK_WAIT_MS must be in range 0..30000"
#endif
#if (W5500_DHCP_RETRY_TIMEOUT_S < 5u) || (W5500_DHCP_RETRY_TIMEOUT_S > 300u)
#error "W5500_DHCP_RETRY_TIMEOUT_S must be in range 5..300"
#endif
#if (W5500_SPI_DMA_ENABLE != 0u) && (W5500_SPI_DMA_ENABLE != 1u)
#error "W5500_SPI_DMA_ENABLE must be 0 or 1"
#endif
#if (W5500_SPI_DMA_MIN_BURST_BYTES < 2u) || (W5500_SPI_DMA_MIN_BURST_BYTES > 2048u)
#error "W5500_SPI_DMA_MIN_BURST_BYTES must be in range 2..2048"
#endif

// Remora.
#if (REMORA_TRANSPORT_RETRY_MS < 50u) || (REMORA_TRANSPORT_RETRY_MS > 5000u)
#error "REMORA_TRANSPORT_RETRY_MS must be in range 50..5000"
#endif
#if (REMORA_POLL_SLEEP_MS > 10u)
#error "REMORA_POLL_SLEEP_MS must be in range 0..10"
#endif
#if (REMORA_RX_BURST_MAX < 1u) || (REMORA_RX_BURST_MAX > 16u)
#error "REMORA_RX_BURST_MAX must be in range 1..16"
#endif

// Anzeige.
#if (DISPLAY_MODEL_SELECT != DISPLAY_MODEL_ST7789) && (DISPLAY_MODEL_SELECT != DISPLAY_MODEL_MCUFRIEND397)
#error "DISPLAY_MODEL_SELECT must be DISPLAY_MODEL_ST7789 or DISPLAY_MODEL_MCUFRIEND397"
#endif

// Touch-Eingabe.
#if (TOUCH_TYPE_SELECT != TOUCH_TYPE_NONE) && (TOUCH_TYPE_SELECT != TOUCH_TYPE_XPT2046)
#error "TOUCH_TYPE_SELECT must be TOUCH_TYPE_NONE or TOUCH_TYPE_XPT2046"
#endif

// Diagnose.
#if (TERMINAL_W5500_ONLY_MODE != 0u) && (TERMINAL_W5500_ONLY_MODE != 1u)
#error "TERMINAL_W5500_ONLY_MODE must be 0 or 1"
#endif
#if (HB_LED_ENABLE != 0u) && (HB_LED_ENABLE != 1u)
#error "HB_LED_ENABLE must be 0 or 1"
#endif
#if (REMORA_HB_TIMEOUT_MS < 50u) || (REMORA_HB_TIMEOUT_MS > 10000u)
#error "REMORA_HB_TIMEOUT_MS must be in range 50..10000"
#endif
#if (HB_LED_BLINK_PERIOD_MS < 20u) || (HB_LED_BLINK_PERIOD_MS > 2000u)
#error "HB_LED_BLINK_PERIOD_MS must be in range 20..2000"
#endif
#if (ENCODER_TERMINAL_REPORT_ENABLE != 0u) && (ENCODER_TERMINAL_REPORT_ENABLE != 1u)
#error "ENCODER_TERMINAL_REPORT_ENABLE must be 0 or 1"
#endif
#if (MT6835_REGISTER_REPORT_ENABLE != 0u) && (MT6835_REGISTER_REPORT_ENABLE != 1u)
#error "MT6835_REGISTER_REPORT_ENABLE must be 0 or 1"
#endif
#if (MT6835_REGISTER_REPORT_INTERVAL_MS < 100u) || (MT6835_REGISTER_REPORT_INTERVAL_MS > 60000u)
#error "MT6835_REGISTER_REPORT_INTERVAL_MS must be in range 100..60000"
#endif
#if (SERIAL_ALIVE_REPORT_ENABLE != 0u) && (SERIAL_ALIVE_REPORT_ENABLE != 1u)
#error "SERIAL_ALIVE_REPORT_ENABLE must be 0 or 1"
#endif

// ============================================================
// 15. PIN-KONFLIKT-GUARDS
// ============================================================

#if PIN_MOTOR_PWM_A == PIN_MOTOR_PWM_B
#error "PIN_MOTOR_PWM_A and PIN_MOTOR_PWM_B must be different"
#endif

#if PIN_MOTOR_ENABLE == PIN_MOTOR_BRAKE
#error "PIN_MOTOR_ENABLE and PIN_MOTOR_BRAKE must be different"
#endif

#if PIN_ST7789_CS == PIN_W5500_CS
#error "PIN_ST7789_CS and PIN_W5500_CS must be different"
#endif

#if PIN_ST7789_DC == PIN_ST7789_RST
#error "PIN_ST7789_DC and PIN_ST7789_RST must be different"
#endif

#if PIN_ST7789_DC == PIN_ST7789_BL
#error "PIN_ST7789_DC and PIN_ST7789_BL must be different"
#endif

#if PIN_ST7789_RST == PIN_ST7789_BL
#error "PIN_ST7789_RST and PIN_ST7789_BL must be different"
#endif

#if W5500_RST_PIN == PIN_ESTOP
#error "W5500_RST_PIN must not collide with PIN_ESTOP"
#endif

#if PIN_W5500_MISO == PIN_W5500_MOSI || PIN_W5500_MISO == PIN_W5500_SCK || PIN_W5500_MISO == PIN_W5500_CS || \
	PIN_W5500_MOSI == PIN_W5500_SCK || PIN_W5500_MOSI == PIN_W5500_CS || PIN_W5500_SCK == PIN_W5500_CS
#error "W5500 SPI pins must all be different"
#endif

#if W5500_RST_PIN == PIN_W5500_MISO || W5500_RST_PIN == PIN_W5500_MOSI || W5500_RST_PIN == PIN_W5500_SCK || W5500_RST_PIN == PIN_W5500_CS
#error "W5500_RST_PIN must not collide with W5500 SPI pins"
#endif

#if MODULE_WS2812_ENABLE
#if PIN_WS2812_DATA == PIN_W5500_CS || PIN_WS2812_DATA == PIN_ST7789_CS || PIN_WS2812_DATA == PIN_TOUCH_CS || \
	PIN_WS2812_DATA == PIN_MOTOR_ENABLE || PIN_WS2812_DATA == PIN_MOTOR_BRAKE
#error "PIN_WS2812_DATA collides with an active control/chip-select pin"
#endif
#endif

#if TOUCH_TYPE_SELECT == TOUCH_TYPE_XPT2046
#if PIN_TOUCH_CS == PIN_W5500_CS || PIN_TOUCH_CS == PIN_ST7789_CS
#error "PIN_TOUCH_CS must not collide with other SPI chip-select pins"
#endif
#endif

#if ENCODER_TYPE_SELECT == ENCODER_TYPE_AS5600
#if PIN_AS5600_SDA == PIN_AS5600_SCL
#error "PIN_AS5600_SDA and PIN_AS5600_SCL must be different"
#endif
#if PIN_AS5600_SDA == PIN_ESTOP || PIN_AS5600_SCL == PIN_ESTOP
#error "AS5600 I2C pins must not collide with PIN_ESTOP"
#endif
#endif

#if ENCODER_TYPE_SELECT == ENCODER_TYPE_MT6835
#if PIN_MT6835_MISO == PIN_MT6835_MOSI || PIN_MT6835_MISO == PIN_MT6835_SCK || PIN_MT6835_MISO == PIN_MT6835_CSN || \
	PIN_MT6835_MOSI == PIN_MT6835_SCK || PIN_MT6835_MOSI == PIN_MT6835_CSN || PIN_MT6835_SCK == PIN_MT6835_CSN
#error "MT6835 SPI pins must all be different"
#endif
#endif

#if defined(BOARD_WAVESHARE_RP2040_ZERO) && MODULE_W5500_ENABLE && (ENCODER_TYPE_SELECT == ENCODER_TYPE_MT6835)
// Zero: W5500 extra nutzt SPI1; MT6835 wird board-spezifisch auf SPI0 gemappt.
#endif

#endif // CONFIG_H


