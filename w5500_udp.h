#ifndef W5500_UDP_H
#define W5500_UDP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool w5500_udp_init(uint16_t local_port, uint8_t socket_num);
size_t w5500_udp_receive(uint8_t* out_buf, size_t max_len);
bool w5500_udp_send(const uint8_t* data, size_t len, uint16_t dst_port);
bool w5500_udp_set_remote_endpoint(const uint8_t ip[4], uint16_t port);
void w5500_udp_get_remote_endpoint(uint8_t ip_out[4], uint16_t* port_out);

#endif // W5500_UDP_H
