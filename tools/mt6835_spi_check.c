/*
Simple MT6835 SPI check utility (RP2040 C, drop into project tools).
Build/usage: add to your CMake for a quick test target or compile separately.
This does a burst read (0x003..0x006) and prints raw angle, status and CRC,
and repeats at a slow rate so you can observe SCK/MISO on a logic analyzer.

Note: Intended as a minimal test harness — integrates with pico-sdk headers.
*/

#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"

#define PIN_MISO 4
#define PIN_MOSI 7
#define PIN_SCK  6
#define PIN_CSN  5
#define SPI_IF   spi0
#define SPI_HZ   16000000u

int main() {
    stdio_init_all();
    sleep_ms(2000);

    spi_init(SPI_IF, SPI_HZ);
    spi_set_format(SPI_IF, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);

    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_init(PIN_CSN);
    gpio_set_dir(PIN_CSN, GPIO_OUT);
    gpio_put(PIN_CSN, 1);

    uint8_t tx[7] = {0};
    uint8_t rx[7] = {0};
    tx[0] = (uint8_t)(0xAu << 4);
    tx[1] = (uint8_t)(0x003u << 4);

    while (true) {
        gpio_put(PIN_CSN, 0);
        spi_write_read_blocking(SPI_IF, tx, rx, sizeof(tx));
        gpio_put(PIN_CSN, 1);

        uint8_t reg003 = rx[2];
        uint8_t reg004 = rx[3];
        uint8_t reg005 = rx[4];
        uint8_t reg006 = rx[5];

        uint32_t angle_raw = ((uint32_t)reg003 << 13) | ((uint32_t)reg004 << 5) | ((uint32_t)(reg005 >> 3) & 0x1Fu);
        uint8_t status = (uint8_t)(reg005 & 0x07u);

        printf("SPI READ: raw=%lu st=0x%02X crc=0x%02X\n", (unsigned long)angle_raw, (unsigned)status, (unsigned)reg006);
        sleep_ms(500);
    }
    return 0;
}
