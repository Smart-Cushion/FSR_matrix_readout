#pragma once

#include "hal/uart_types.h"
#include "esp_err.h"
#include "soc/gpio_num.h"
#include <stddef.h>
#include <stdint.h>

static constexpr uint32_t FSR_WIRESENS_DEFAULT_BAUD_RATE = 921600u;
static constexpr uint16_t FSR_WIRESENS_DEFAULT_NODES_PER_PACKET = 64u;
static constexpr uint16_t FSR_WIRESENS_MAX_NODES_PER_PACKET = 120u;

typedef struct {
    uart_port_t uart_num;
    gpio_num_t tx_gpio;
    gpio_num_t rx_gpio;
    uint32_t baud_rate;
    uint8_t sensor_id;
    uint16_t nodes_per_packet;
    size_t tx_buffer_size;
} fsr_wiresens_cfg_t;

fsr_wiresens_cfg_t fsr_wiresens_default_cfg(uart_port_t uart_num,
                                            gpio_num_t tx_gpio,
                                            gpio_num_t rx_gpio);

void fsr_wiresens_init(fsr_wiresens_cfg_t cfg);

size_t fsr_wiresens_packet_payload_size(uint16_t nodes_per_packet);
size_t fsr_wiresens_packet_serial_size(uint16_t nodes_per_packet);

esp_err_t fsr_wiresens_pack_packet(uint8_t *dst,
                                   size_t dst_size,
                                   uint8_t sensor_id,
                                   uint16_t start_idx,
                                   const uint16_t *readings,
                                   uint16_t nodes_per_packet,
                                   uint32_t packet_number);

esp_err_t fsr_wiresens_send_frame(const uint16_t *frame);
