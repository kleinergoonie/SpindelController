#ifndef REMORA_IFACE_H
#define REMORA_IFACE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"

// Referenz: third_party/Remora-ref (feature/ethernet)
#define REMORA_SRC_PORT REMORA_SERVER_PORT
#define REMORA_DST_PORT REMORA_SERVER_PORT
#define REMORA_PACKET_SIZE 64u

#define REMORA_PRU_DATA        0x64617461u // "data"
#define REMORA_PRU_READ        0x72656164u // "read"
#define REMORA_PRU_WRITE       0x77726974u // "writ"
#define REMORA_PRU_ACKNOWLEDGE 0x61636b6eu // "ackn"
#define REMORA_PRU_ERR         0x6572726fu // "erro"

typedef struct {
    float speed_setpoint_rpm;
    float position_setpoint_deg;
    uint8_t control_mode; // 0=Drehzahl, 1=Position
    uint8_t direction; // 0=CW, 1=CCW
    uint8_t brake_cmd; // 0=Freilauf, 1=aktive Bremse
    bool enable;
} remora_command_t;

typedef struct {
    float speed_measured_rpm;
    uint32_t status_bits;
} remora_feedback_t;

void remora_iface_init(void);
void remora_iface_poll(void);
bool remora_iface_get_latest_command(remora_command_t* out_cmd);
void remora_iface_publish_feedback(const remora_feedback_t* feedback);
bool remora_iface_set_server_endpoint(const uint8_t ip[4], uint16_t port);
void remora_iface_get_server_endpoint(uint8_t ip_out[4], uint16_t* port_out);
uint32_t remora_iface_get_rx_age_ms(uint32_t now_ms);
bool remora_iface_watchdog_safe(void);

// Transportseitige Hooks: UDP/W5500 RX/TX an diese Funktionen anbinden.
void remora_iface_on_rx_packet(const uint8_t* packet, size_t len);
size_t remora_iface_get_tx_packet(uint8_t* out_packet, size_t max_len);

#endif // REMORA_IFACE_H

