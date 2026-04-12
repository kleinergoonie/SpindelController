#include "w5500_udp.h"

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#define _WIZCHIP_ W5500
#define _WIZCHIP_IO_MODE_ _WIZCHIP_IO_MODE_SPI_VDM_
#include "dhcp.h"
#include "socket.h"
#include "w5500.h"
#include "wizchip_conf.h"

static uint8_t g_sock_num;
static uint16_t g_local_port;
static uint8_t g_remote_ip[4] = {
    REMORA_SERVER_IP_0,
    REMORA_SERVER_IP_1,
    REMORA_SERVER_IP_2,
    REMORA_SERVER_IP_3,
};
static uint16_t g_remote_port = REMORA_SERVER_PORT;

static bool g_ready;
static uint32_t g_irq_state;
static bool g_dhcp_active;
static uint32_t g_last_dhcp_tick_ms;
static uint32_t g_dhcp_start_ms;
static uint8_t g_dhcp_socket;
static uint8_t g_dhcp_buf[1024];
static int8_t g_phy_link_prev;
static uint8_t g_dhcp_state_prev;
static uint32_t g_last_rx_err_log_ms;
static uint32_t g_last_tx_err_log_ms;

#if W5500_SPI_DMA_ENABLE
static int g_spi_dma_tx_chan = -1;
static int g_spi_dma_rx_chan = -1;
static bool g_spi_dma_ready;
static uint8_t g_spi_dma_dummy_tx = 0xFFu;
static uint8_t g_spi_dma_dummy_rx;
#endif

#if REMORA_DEBUG_ENABLE
static const char* w5500_dhcp_state_name(uint8_t state) {
    switch (state) {
        case DHCP_STOPPED: return "STOPPED";
        case DHCP_RUNNING: return "RUNNING";
        case DHCP_IP_LEASED: return "IP_LEASED";
        case DHCP_IP_CHANGED: return "IP_CHANGED";
        case DHCP_FAILED: return "FAILED";
        case DHCP_IP_ASSIGN: return "IP_ASSIGN";
        default: return "UNKNOWN";
    }
}

static void w5500_log_dhcp_state_if_changed(uint8_t state) {
    if (state != g_dhcp_state_prev) {
        printf("W5500 DHCP state: %s (0x%02X)\n", w5500_dhcp_state_name(state), (unsigned)state);
        g_dhcp_state_prev = state;
    }
}

static void w5500_log_static_target_config(void) {
    printf("W5500 static target ip=%u.%u.%u.%u gw=%u.%u.%u.%u sn=%u.%u.%u.%u\n",
           (unsigned)W5500_IP_0,
           (unsigned)W5500_IP_1,
           (unsigned)W5500_IP_2,
           (unsigned)W5500_IP_3,
           (unsigned)W5500_GW_0,
           (unsigned)W5500_GW_1,
           (unsigned)W5500_GW_2,
           (unsigned)W5500_GW_3,
           (unsigned)W5500_SN_0,
           (unsigned)W5500_SN_1,
           (unsigned)W5500_SN_2,
           (unsigned)W5500_SN_3);
}

static void w5500_log_netinfo(const char* prefix) {
    wiz_NetInfo info = {0};
    wizchip_getnetinfo(&info);
    printf("W5500 %s mode=%s ip=%u.%u.%u.%u gw=%u.%u.%u.%u sn=%u.%u.%u.%u mac=%02X:%02X:%02X:%02X:%02X:%02X\n",
           prefix,
           g_dhcp_active ? "dhcp" : "static",
           (unsigned)info.ip[0],
           (unsigned)info.ip[1],
           (unsigned)info.ip[2],
           (unsigned)info.ip[3],
           (unsigned)info.gw[0],
           (unsigned)info.gw[1],
           (unsigned)info.gw[2],
           (unsigned)info.gw[3],
           (unsigned)info.sn[0],
           (unsigned)info.sn[1],
           (unsigned)info.sn[2],
           (unsigned)info.sn[3],
           (unsigned)info.mac[0],
           (unsigned)info.mac[1],
           (unsigned)info.mac[2],
           (unsigned)info.mac[3],
           (unsigned)info.mac[4],
           (unsigned)info.mac[5]);
}

static void w5500_log_phy_link_if_changed(void) {
    const int8_t link = wizphy_getphylink();
    if (link != g_phy_link_prev) {
        printf("W5500 PHY link %s\n", (link == PHY_LINK_ON) ? "UP" : "DOWN");
        g_phy_link_prev = link;
    }
}
#else
static void w5500_log_dhcp_state_if_changed(uint8_t state) {
    (void)state;
}

static void w5500_log_phy_link_if_changed(void) {
}
#endif

static bool ip_equal4(const uint8_t a[4], const uint8_t b[4]) {
    return (a[0] == b[0]) && (a[1] == b[1]) && (a[2] == b[2]) && (a[3] == b[3]);
}

static void wiz_critical_enter(void) {
    g_irq_state = save_and_disable_interrupts();
}

static void wiz_critical_exit(void) {
    restore_interrupts(g_irq_state);
}

static void wiz_cs_select(void) {
    gpio_put(PIN_W5500_CS, 0);
}

static void wiz_cs_deselect(void) {
    gpio_put(PIN_W5500_CS, 1);
}

static uint8_t wiz_spi_readbyte(void) {
    uint8_t tx = 0xFF;
    uint8_t rx = 0x00;
    spi_write_read_blocking(W5500_SPI_IF, &tx, &rx, 1);
    return rx;
}

static void wiz_spi_writebyte(uint8_t wb) {
    spi_write_blocking(W5500_SPI_IF, &wb, 1);
}

static void wiz_spi_readburst(uint8_t* pBuf, uint16_t len) {
#if W5500_SPI_DMA_ENABLE
    if (g_spi_dma_ready && (len >= W5500_SPI_DMA_MIN_BURST_BYTES)) {
        dma_channel_config c_tx = dma_channel_get_default_config((uint)g_spi_dma_tx_chan);
        channel_config_set_transfer_data_size(&c_tx, DMA_SIZE_8);
        channel_config_set_read_increment(&c_tx, false);
        channel_config_set_write_increment(&c_tx, false);
        channel_config_set_dreq(&c_tx, spi_get_dreq(W5500_SPI_IF, true));

        dma_channel_config c_rx = dma_channel_get_default_config((uint)g_spi_dma_rx_chan);
        channel_config_set_transfer_data_size(&c_rx, DMA_SIZE_8);
        channel_config_set_read_increment(&c_rx, false);
        channel_config_set_write_increment(&c_rx, true);
        channel_config_set_dreq(&c_rx, spi_get_dreq(W5500_SPI_IF, false));

        dma_channel_configure((uint)g_spi_dma_rx_chan,
                              &c_rx,
                              pBuf,
                              &spi_get_hw(W5500_SPI_IF)->dr,
                              len,
                              false);
        dma_channel_configure((uint)g_spi_dma_tx_chan,
                              &c_tx,
                              &spi_get_hw(W5500_SPI_IF)->dr,
                              &g_spi_dma_dummy_tx,
                              len,
                              false);

        dma_start_channel_mask((1u << g_spi_dma_rx_chan) | (1u << g_spi_dma_tx_chan));
        dma_channel_wait_for_finish_blocking((uint)g_spi_dma_tx_chan);
        dma_channel_wait_for_finish_blocking((uint)g_spi_dma_rx_chan);
        return;
    }
#endif

    for (uint16_t i = 0; i < len; ++i) {
        pBuf[i] = wiz_spi_readbyte();
    }
}

static void wiz_spi_writeburst(uint8_t* pBuf, uint16_t len) {
#if W5500_SPI_DMA_ENABLE
    if (g_spi_dma_ready && (len >= W5500_SPI_DMA_MIN_BURST_BYTES)) {
        dma_channel_config c_tx = dma_channel_get_default_config((uint)g_spi_dma_tx_chan);
        channel_config_set_transfer_data_size(&c_tx, DMA_SIZE_8);
        channel_config_set_read_increment(&c_tx, true);
        channel_config_set_write_increment(&c_tx, false);
        channel_config_set_dreq(&c_tx, spi_get_dreq(W5500_SPI_IF, true));

        dma_channel_config c_rx = dma_channel_get_default_config((uint)g_spi_dma_rx_chan);
        channel_config_set_transfer_data_size(&c_rx, DMA_SIZE_8);
        channel_config_set_read_increment(&c_rx, false);
        channel_config_set_write_increment(&c_rx, false);
        channel_config_set_dreq(&c_rx, spi_get_dreq(W5500_SPI_IF, false));

        dma_channel_configure((uint)g_spi_dma_rx_chan,
                              &c_rx,
                              &g_spi_dma_dummy_rx,
                              &spi_get_hw(W5500_SPI_IF)->dr,
                              len,
                              false);
        dma_channel_configure((uint)g_spi_dma_tx_chan,
                              &c_tx,
                              &spi_get_hw(W5500_SPI_IF)->dr,
                              pBuf,
                              len,
                              false);

        dma_start_channel_mask((1u << g_spi_dma_rx_chan) | (1u << g_spi_dma_tx_chan));
        dma_channel_wait_for_finish_blocking((uint)g_spi_dma_tx_chan);
        dma_channel_wait_for_finish_blocking((uint)g_spi_dma_rx_chan);
        return;
    }
#endif

    spi_write_blocking(W5500_SPI_IF, pBuf, len);
}

static void w5500_apply_static_netinfo(void) {
    wiz_NetInfo net_info = {
        .mac = {W5500_MAC_0, W5500_MAC_1, W5500_MAC_2, W5500_MAC_3, W5500_MAC_4, W5500_MAC_5},
        .ip = {W5500_IP_0, W5500_IP_1, W5500_IP_2, W5500_IP_3},
        .sn = {W5500_SN_0, W5500_SN_1, W5500_SN_2, W5500_SN_3},
        .gw = {W5500_GW_0, W5500_GW_1, W5500_GW_2, W5500_GW_3},
        .dns = {0, 0, 0, 0},
        .dhcp = NETINFO_STATIC,
    };
    wizchip_setnetinfo(&net_info);
}

static bool w5500_netinfo_looks_valid(void) {
    wiz_NetInfo info = {0};
    wizchip_getnetinfo(&info);

    const bool ip_zero = (info.ip[0] == 0u) && (info.ip[1] == 0u) && (info.ip[2] == 0u) && (info.ip[3] == 0u);
    const bool gw_zero = (info.gw[0] == 0u) && (info.gw[1] == 0u) && (info.gw[2] == 0u) && (info.gw[3] == 0u);
    const bool sn_zero = (info.sn[0] == 0u) && (info.sn[1] == 0u) && (info.sn[2] == 0u) && (info.sn[3] == 0u);

    return !(ip_zero && gw_zero && sn_zero);
}

static bool w5500_open_udp_socket(void) {
    (void)close(g_sock_num);
    const int32_t ret = socket(g_sock_num, Sn_MR_UDP, g_local_port, SF_IO_NONBLOCK);
#if REMORA_DEBUG_ENABLE
    if (ret < 0) {
        printf("W5500 socket open failed sock=%u port=%u ret=%ld sr=0x%02X\n",
               (unsigned)g_sock_num,
               (unsigned)g_local_port,
               (long)ret,
               (unsigned)getSn_SR(g_sock_num));
    }
#endif
    return ret >= 0;
}

static void w5500_dhcp_start_client(void) {
#if W5500_USE_DHCP
    DHCP_init(g_dhcp_socket, g_dhcp_buf);
    g_last_dhcp_tick_ms = to_ms_since_boot(get_absolute_time());
    g_dhcp_start_ms = g_last_dhcp_tick_ms;
#if REMORA_DEBUG_ENABLE
    printf("W5500 DHCP start socket=%u timeout=%us fallback_static=%u retry=%us\n",
           (unsigned)g_dhcp_socket,
           (unsigned)W5500_DHCP_TIMEOUT_S,
           (unsigned)W5500_DHCP_FALLBACK_STATIC,
           (unsigned)W5500_DHCP_RETRY_TIMEOUT_S);
#endif
#endif
}

static void w5500_wait_for_phy_link(void) {
    w5500_log_phy_link_if_changed();

#if W5500_PHY_LINK_WAIT_MS > 0u
    const uint32_t start_ms = to_ms_since_boot(get_absolute_time());
    while ((to_ms_since_boot(get_absolute_time()) - start_ms) < W5500_PHY_LINK_WAIT_MS) {
        if (wizphy_getphylink() == PHY_LINK_ON) {
            break;
        }
        sleep_ms(50);
    }

    w5500_log_phy_link_if_changed();
#if REMORA_DEBUG_ENABLE
    printf("W5500 PHYCFGR=0x%02X after wait=%ums\n",
           (unsigned)getPHYCFGR(),
           (unsigned)W5500_PHY_LINK_WAIT_MS);
#endif
#endif
}

static void w5500_dhcp_service(void) {
#if W5500_USE_DHCP
    if (!g_dhcp_active) {
        return;
    }

    const uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    while ((now_ms - g_last_dhcp_tick_ms) >= 1000u) {
        DHCP_time_handler();
        g_last_dhcp_tick_ms += 1000u;
        w5500_log_phy_link_if_changed();
    }

    const uint8_t dhcp_state = DHCP_run();
    w5500_log_dhcp_state_if_changed(dhcp_state);
    if ((dhcp_state == DHCP_IP_ASSIGN) || (dhcp_state == DHCP_IP_CHANGED)) {
        if (!w5500_open_udp_socket()) {
            g_ready = false;
        }
#if REMORA_DEBUG_ENABLE
        else {
            w5500_log_netinfo("DHCP lease/update");
            printf("W5500 UDP socket reopened on %u\n", (unsigned)g_local_port);
        }
#endif
            } else if (dhcp_state == DHCP_IP_LEASED) {
        #if REMORA_DEBUG_ENABLE
            w5500_log_netinfo("DHCP leased");
        #endif
            } else if (dhcp_state == DHCP_FAILED) {
        #if REMORA_DEBUG_ENABLE
            printf("W5500 DHCP failed, restarting client\n");
        #endif
        #if W5500_DHCP_FALLBACK_STATIC
            w5500_apply_static_netinfo();
        #if REMORA_DEBUG_ENABLE
            w5500_log_netinfo("fallback static");
        #endif
        #endif
            DHCP_stop();
            w5500_dhcp_start_client();
            } else if ((dhcp_state == DHCP_RUNNING) &&
                   ((now_ms - g_dhcp_start_ms) >= (W5500_DHCP_RETRY_TIMEOUT_S * 1000u))) {
        #if REMORA_DEBUG_ENABLE
            printf("W5500 DHCP still running after %us, restarting client\n", (unsigned)W5500_DHCP_RETRY_TIMEOUT_S);
        #endif
        #if W5500_DHCP_FALLBACK_STATIC
            w5500_apply_static_netinfo();
        #if REMORA_DEBUG_ENABLE
            w5500_log_netinfo("fallback static");
        #endif
        #endif
            DHCP_stop();
            w5500_dhcp_start_client();
    }
#else
    return;
#endif
}

bool w5500_udp_init(uint16_t local_port, uint8_t socket_num) {
    g_sock_num = socket_num;
    g_local_port = local_port;
    g_dhcp_active = false;
    g_phy_link_prev = -1;
    g_dhcp_state_prev = 0xFFu;
    g_last_rx_err_log_ms = 0u;
    g_last_tx_err_log_ms = 0u;

#if W5500_SPI_DMA_ENABLE
    if (!g_spi_dma_ready) {
        g_spi_dma_tx_chan = dma_claim_unused_channel(false);
        g_spi_dma_rx_chan = dma_claim_unused_channel(false);
        g_spi_dma_ready = (g_spi_dma_tx_chan >= 0) && (g_spi_dma_rx_chan >= 0);
#if REMORA_DEBUG_ENABLE
        if (g_spi_dma_ready) {
            printf("W5500 SPI DMA enabled tx=%d rx=%d min_burst=%u\n",
                   g_spi_dma_tx_chan,
                   g_spi_dma_rx_chan,
                   (unsigned)W5500_SPI_DMA_MIN_BURST_BYTES);
        } else {
            printf("W5500 SPI DMA unavailable, fallback to blocking SPI\n");
        }
#endif
    }
#endif

    gpio_init(W5500_RST_PIN);
    gpio_set_dir(W5500_RST_PIN, GPIO_OUT);
    gpio_put(W5500_RST_PIN, 0);
    sleep_ms(100);
    gpio_put(W5500_RST_PIN, 1);
    sleep_ms(100);
#if REMORA_DEBUG_ENABLE
    printf("W5500 HW reset pulse on GPIO %u (ref timing 100/100ms)\n", (unsigned)W5500_RST_PIN);
#endif

    gpio_init(PIN_W5500_CS);
    gpio_set_dir(PIN_W5500_CS, GPIO_OUT);
    gpio_put(PIN_W5500_CS, 1);

    spi_init(W5500_SPI_IF, W5500_SPI_HZ);
    spi_set_format(W5500_SPI_IF, 8u, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(PIN_W5500_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_W5500_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_W5500_MISO, GPIO_FUNC_SPI);
#if REMORA_DEBUG_ENABLE
    w5500_log_static_target_config();
    printf("W5500 SPI configured hz=%lu sck=%u mosi=%u miso=%u cs=%u rst=%u\n",
           (unsigned long)W5500_SPI_HZ,
           (unsigned)PIN_W5500_SCK,
           (unsigned)PIN_W5500_MOSI,
           (unsigned)PIN_W5500_MISO,
           (unsigned)PIN_W5500_CS,
           (unsigned)W5500_RST_PIN);
#if W5500_USE_DHCP
    printf("W5500 DHCP configured enabled=1 timeout=%us fallback_static=%u\n",
           (unsigned)W5500_DHCP_TIMEOUT_S,
           (unsigned)W5500_DHCP_FALLBACK_STATIC);
#else
    printf("W5500 DHCP configured enabled=0 (static mode)\n");
#endif
#endif

    reg_wizchip_cris_cbfunc(wiz_critical_enter, wiz_critical_exit);
    reg_wizchip_cs_cbfunc(wiz_cs_select, wiz_cs_deselect);
    reg_wizchip_spi_cbfunc(wiz_spi_readbyte, wiz_spi_writebyte);
    reg_wizchip_spiburst_cbfunc(wiz_spi_readburst, wiz_spi_writeburst);

    uint8_t txsize[8] = {2,2,2,2,2,2,2,2};
    uint8_t rxsize[8] = {2,2,2,2,2,2,2,2};

    if (wizchip_init(txsize, rxsize) != 0) {
#if REMORA_DEBUG_ENABLE
        printf("W5500 wizchip_init failed\n");
#endif
        g_ready = false;
        return false;
    }

    const uint8_t version = getVERSIONR();
#if REMORA_DEBUG_ENABLE
    printf("W5500 version register: 0x%02X\n", (unsigned)version);
#endif
    if (version != 0x04u) {
#if REMORA_DEBUG_ENABLE
        printf("W5500 version mismatch (expected 0x04)\n");
#endif
        g_ready = false;
        return false;
    }

    w5500_wait_for_phy_link();

    w5500_apply_static_netinfo();
#if REMORA_DEBUG_ENABLE
    w5500_log_netinfo("static preset");
#endif

    // Wenn netinfo nur Nullen liefert, ist die SPI/W5500-Kommunikation vermutlich defekt.
    // In diesem Zustand socket() nicht aufrufen, um Start-Haenger zu vermeiden.
    if (!w5500_netinfo_looks_valid()) {
#if REMORA_DEBUG_ENABLE
        printf("W5500 netinfo invalid after static preset (all zero)\n");
#endif
        g_ready = false;
        return false;
    }

#if W5500_USE_DHCP
    g_dhcp_socket = (g_sock_num == 7u) ? 6u : 7u;
    g_dhcp_active = true;
    w5500_dhcp_start_client();

    bool dhcp_ok = false;
    const uint32_t deadline_ms = g_last_dhcp_tick_ms + (W5500_DHCP_TIMEOUT_S * 1000u);
    while ((int32_t)(to_ms_since_boot(get_absolute_time()) - deadline_ms) < 0) {
        const uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        while ((now_ms - g_last_dhcp_tick_ms) >= 1000u) {
            DHCP_time_handler();
            g_last_dhcp_tick_ms += 1000u;
        }

        const uint8_t state = DHCP_run();
        w5500_log_dhcp_state_if_changed(state);
        if ((state == DHCP_IP_ASSIGN) || (state == DHCP_IP_CHANGED) || (state == DHCP_IP_LEASED)) {
            dhcp_ok = true;
            break;
        }
        sleep_ms(10);
    }

    if (dhcp_ok) {
#if REMORA_DEBUG_ENABLE
        w5500_log_netinfo("DHCP ready");
#endif
    } else {
#if REMORA_DEBUG_ENABLE
        printf("W5500 DHCP timeout after %us\n", (unsigned)W5500_DHCP_TIMEOUT_S);
#endif
#if W5500_DHCP_FALLBACK_STATIC
        w5500_apply_static_netinfo();
#if REMORA_DEBUG_ENABLE
        w5500_log_netinfo("fallback static");
        printf("W5500 DHCP continues in background while static fallback is active\n");
#endif
#else
        g_ready = false;
        return false;
#endif
    }
#endif

    if (!w5500_open_udp_socket()) {
        g_ready = false;
        return false;
    }

#if REMORA_DEBUG_ENABLE
    printf("W5500 UDP socket open local_port=%u remote=%u.%u.%u.%u:%u\n",
           (unsigned)g_local_port,
           (unsigned)g_remote_ip[0],
           (unsigned)g_remote_ip[1],
           (unsigned)g_remote_ip[2],
           (unsigned)g_remote_ip[3],
           (unsigned)g_remote_port);
#endif

    g_ready = true;
    return true;
}

size_t w5500_udp_receive(uint8_t* out_buf, size_t max_len) {
    if (!g_ready || !out_buf || max_len == 0u) {
        return 0u;
    }

    w5500_dhcp_service();

    uint16_t src_port = 0u;
    uint8_t src_ip[4] = {0};
    const int32_t ret = recvfrom(g_sock_num, out_buf, (uint16_t)max_len, src_ip, &src_port);

    if (ret <= 0) {
#if REMORA_DEBUG_ENABLE
        if (ret < 0) {
            const uint32_t now_ms = to_ms_since_boot(get_absolute_time());
            if ((now_ms - g_last_rx_err_log_ms) >= 1000u) {
                printf("W5500 recvfrom error ret=%ld sr=0x%02X\n", (long)ret, (unsigned)getSn_SR(g_sock_num));
                g_last_rx_err_log_ms = now_ms;
            }
        }
#endif
        return 0u;
    }

#if REMORA_STRICT_HOST_FILTER
    if (!ip_equal4(src_ip, g_remote_ip) || (src_port != g_remote_port)) {
        return 0u;
    }
#endif

    // Letzten Absender als Antwortziel behalten (LinuxCNC-Host).
    memcpy(g_remote_ip, src_ip, sizeof(g_remote_ip));
    (void)src_port;

    return (size_t)ret;
}

bool w5500_udp_send(const uint8_t* data, size_t len, uint16_t dst_port) {
    if (!g_ready || !data || len == 0u || len > 65535u) {
        return false;
    }

    w5500_dhcp_service();

    const int32_t ret = sendto(g_sock_num, (uint8_t*)data, (uint16_t)len, g_remote_ip, dst_port);
#if REMORA_DEBUG_ENABLE
    if (ret <= 0) {
        const uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        if ((now_ms - g_last_tx_err_log_ms) >= 1000u) {
            printf("W5500 sendto error ret=%ld dst=%u.%u.%u.%u:%u sr=0x%02X\n",
                   (long)ret,
                   (unsigned)g_remote_ip[0],
                   (unsigned)g_remote_ip[1],
                   (unsigned)g_remote_ip[2],
                   (unsigned)g_remote_ip[3],
                   (unsigned)dst_port,
                   (unsigned)getSn_SR(g_sock_num));
            g_last_tx_err_log_ms = now_ms;
        }
    }
#endif
    return ret > 0;
}

bool w5500_udp_set_remote_endpoint(const uint8_t ip[4], uint16_t port) {
    if (!ip || port == 0u) {
        return false;
    }

    memcpy(g_remote_ip, ip, 4u);
    g_remote_port = port;
    return true;
}

void w5500_udp_get_remote_endpoint(uint8_t ip_out[4], uint16_t* port_out) {
    if (ip_out) {
        memcpy(ip_out, g_remote_ip, 4u);
    }
    if (port_out) {
        *port_out = g_remote_port;
    }
}

