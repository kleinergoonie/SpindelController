#include "remora_iface.h"

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "pico/time.h"
#include "w5500_udp.h"

// Platzhalter fuer Remora-over-Ethernet-Integration mit W5500.
// Aktuelle Implementierung haelt ein Standardkommando vor und erlaubt spaetere Parser-Erweiterung.

static remora_command_t g_cmd;
static uint8_t g_tx_packet[REMORA_PACKET_SIZE];
static size_t g_tx_packet_len;
static bool g_transport_ok;
static bool g_transport_disabled;
static uint32_t g_last_debug_ms;
static uint32_t g_last_transport_retry_ms;
static uint32_t g_last_rx_ms;
static uint32_t g_last_heartbeat_ms;

static uint32_t read_u32_le(const uint8_t* p) {
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void write_u32_le(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static float read_f32_le(const uint8_t* p) {
    float out;
    memcpy(&out, p, sizeof(out));
    return out;
}

static void write_f32_le(uint8_t* p, float v) {
    memcpy(p, &v, sizeof(v));
}

static uint32_t get_millis_now(void) {
    return to_ms_since_boot(get_absolute_time());
}

static void remora_debug_frame(const char* dir, uint32_t header, size_t len) {
#if REMORA_DEBUG_ENABLE && !TERMINAL_W5500_ONLY_MODE
    const uint32_t now = get_millis_now();
    if ((now - g_last_debug_ms) >= REMORA_DEBUG_PRINT_INTERVAL_MS) {
        printf("REMORA %s hdr=0x%08lx len=%u\n", dir, (unsigned long)header, (unsigned)len);
        g_last_debug_ms = now;
    }
#else
    (void)dir;
    (void)header;
    (void)len;
#endif
}

void remora_iface_init(void) {
    memset(&g_cmd, 0, sizeof(g_cmd));
    memset(g_tx_packet, 0, sizeof(g_tx_packet));
    g_tx_packet_len = 0u;
#if MODULE_W5500_ENABLE
    g_transport_ok = w5500_udp_init(REMORA_SRC_PORT, 0u);
    g_transport_disabled = (!g_transport_ok) && HARDWARETEST_MODE;
#else
    g_transport_ok = false;
    g_transport_disabled = true;
#endif
    g_last_debug_ms = 0u;
    g_last_transport_retry_ms = 0u;
    g_last_rx_ms = get_millis_now();
    g_last_heartbeat_ms = 0u;

#if REMORA_DEBUG_ENABLE && !TERMINAL_W5500_ONLY_MODE
    if (g_transport_ok) {
        printf("REMORA transport ready on UDP/%u\n", (unsigned)REMORA_SRC_PORT);
    } else if (g_transport_disabled) {
        printf("REMORA transport disabled (hardwaretest mode, W5500 missing)\n");
    } else {
        printf("REMORA transport not ready, retry active\n");
    }
#endif
}
bool remora_iface_set_server_endpoint(const uint8_t ip[4], uint16_t port) {
    if (!ip || (port == 0u)) {
        return false;
    }

    return w5500_udp_set_remote_endpoint(ip, port);
}

void remora_iface_get_server_endpoint(uint8_t ip_out[4], uint16_t* port_out) {
    w5500_udp_get_remote_endpoint(ip_out, port_out);
}

uint32_t remora_iface_get_rx_age_ms(uint32_t now_ms) {
    if (g_transport_disabled || !g_transport_ok) {
        return UINT32_MAX;
    }
    return now_ms - g_last_rx_ms;
}

bool remora_iface_watchdog_safe(void) {
    return g_transport_ok || g_transport_disabled;
}

void remora_iface_poll(void) {
    if (g_transport_disabled) {
        return;
    }

    if (!g_transport_ok) {
        const uint32_t now = get_millis_now();
        if ((now - g_last_transport_retry_ms) >= REMORA_TRANSPORT_RETRY_MS) {
            g_transport_ok = w5500_udp_init(REMORA_SRC_PORT, 0u);
            g_last_transport_retry_ms = now;
#if REMORA_DEBUG_ENABLE && !TERMINAL_W5500_ONLY_MODE
            if (g_transport_ok) {
                printf("REMORA transport recovered on UDP/%u\n", (unsigned)REMORA_SRC_PORT);
            }
#endif
        }
        return;
    }

#if REMORA_DEBUG_ENABLE && !TERMINAL_W5500_ONLY_MODE
    const uint32_t now = get_millis_now();
    if ((now - g_last_heartbeat_ms) >= 1000u) {
        uint8_t ip[4] = {0u, 0u, 0u, 0u};
        uint16_t port = 0u;
        w5500_udp_get_remote_endpoint(ip, &port);
        printf("REMORA heartbeat t=%lums rx_age=%lums endpoint=%u.%u.%u.%u:%u\n",
               (unsigned long)now,
               (unsigned long)(now - g_last_rx_ms),
               (unsigned)ip[0],
               (unsigned)ip[1],
               (unsigned)ip[2],
               (unsigned)ip[3],
               (unsigned)port);
        g_last_heartbeat_ms = now;
    }
#endif

    for (uint32_t i = 0u; i < REMORA_RX_BURST_MAX; ++i) {
        uint8_t rx_packet[REMORA_PACKET_SIZE];
        const size_t rx_len = w5500_udp_receive(rx_packet, sizeof(rx_packet));
        if (rx_len == 0u) {
            break;
        }

        g_last_rx_ms = get_millis_now();
        const uint32_t rx_header = read_u32_le(rx_packet);
        remora_debug_frame("RX", rx_header, rx_len);

        remora_iface_on_rx_packet(rx_packet, rx_len);
        uint8_t tx_packet[REMORA_PACKET_SIZE];
        const size_t tx_len = remora_iface_get_tx_packet(tx_packet, sizeof(tx_packet));
        if (tx_len > 0u) {
            const uint32_t tx_header = read_u32_le(tx_packet);
            remora_debug_frame("TX", tx_header, tx_len);
            uint16_t dst_port = REMORA_DST_PORT;
            w5500_udp_get_remote_endpoint(NULL, &dst_port);
            (void)w5500_udp_send(tx_packet, tx_len, dst_port);
        }
    }
}

bool remora_iface_get_latest_command(remora_command_t* out_cmd) {
    if (!out_cmd) {
        return false;
    }
    *out_cmd = g_cmd;
    return true;
}

void remora_iface_publish_feedback(const remora_feedback_t* feedback) {
    if (!feedback) {
        return;
    }

    // 64-Byte-"data"-Frame aufbauen, kompatibel zum Remora-Read-Response-Ablauf.
    memset(g_tx_packet, 0, sizeof(g_tx_packet));
    write_u32_le(&g_tx_packet[0], REMORA_PRU_DATA);
    write_f32_le(&g_tx_packet[4], feedback->speed_measured_rpm);
    write_u32_le(&g_tx_packet[8], feedback->status_bits);
    g_tx_packet_len = REMORA_PACKET_SIZE;
}

void remora_iface_on_rx_packet(const uint8_t* packet, size_t len) {
    if (!packet || len < 4u) {
        return;
    }

    const uint32_t header = read_u32_le(packet);

    if (header == REMORA_PRU_WRITE && len >= REMORA_PACKET_SIZE) {
        // Minimale Kommandoabbildung fuer das Spindelprojekt.
        // [4..7]  float speed_setpoint_rpm
        // [12..15] float position_setpoint_deg (optional, fuer Positionsmodus)
        // [8]     direction (0/1)
        // [9]     brake_cmd (0/1)
        // [10]    enable (0/1)
        // [11]    control_mode (0=speed, 1=position)
        g_cmd.speed_setpoint_rpm = read_f32_le(&packet[4]);
        if (g_cmd.speed_setpoint_rpm < SPINDLE_RPM_MIN) {
            g_cmd.speed_setpoint_rpm = SPINDLE_RPM_MIN;
        }
        if (g_cmd.speed_setpoint_rpm > SPINDLE_RPM_MAX) {
            g_cmd.speed_setpoint_rpm = SPINDLE_RPM_MAX;
        }
        g_cmd.position_setpoint_deg = read_f32_le(&packet[12]);
        g_cmd.control_mode = (packet[11] != 0u) ? CONTROL_MODE_POSITION : CONTROL_MODE_SPEED;
        g_cmd.direction = (packet[8] != 0u) ? 1u : 0u;
        g_cmd.brake_cmd = (packet[9] != 0u) ? 1u : 0u;
        g_cmd.enable = (packet[10] != 0u);

        // ACK fuer Schreibanfrage vorbereiten.
        memset(g_tx_packet, 0, sizeof(g_tx_packet));
        write_u32_le(&g_tx_packet[0], REMORA_PRU_ACKNOWLEDGE);
        g_tx_packet_len = 4u;
    } else if (header == REMORA_PRU_READ && len >= 4u) {
        // Letztes von publish_feedback() aufgebautes REMORA_PRU_DATA-Paket beibehalten.
        if (g_tx_packet_len == 0u) {
            memset(g_tx_packet, 0, sizeof(g_tx_packet));
            write_u32_le(&g_tx_packet[0], REMORA_PRU_DATA);
            g_tx_packet_len = REMORA_PACKET_SIZE;
        }
    } else {
        memset(g_tx_packet, 0, sizeof(g_tx_packet));
        write_u32_le(&g_tx_packet[0], REMORA_PRU_ERR);
        g_tx_packet_len = 4u;
    }
}

size_t remora_iface_get_tx_packet(uint8_t* out_packet, size_t max_len) {
    if (!out_packet || g_tx_packet_len == 0u || max_len < g_tx_packet_len) {
        return 0u;
    }
    memcpy(out_packet, g_tx_packet, g_tx_packet_len);
    return g_tx_packet_len;
}

