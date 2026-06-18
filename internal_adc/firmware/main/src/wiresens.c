#include "wiresens.h"
#include "esp_check.h"
#include "esp_err.h"
#include "pin_defs.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "driver/uart.h"

// WiReSens serial packet format:
//
//   payload: send_id + start_idx + readings[nodes_per_packet] + packet_number
//   serial:  payload + "wr"
//
// Field layout:
//
//   uint8_t  send_id
//   uint16_t start_idx
//   uint16_t readings[nodes_per_packet]
//   uint32_t packet_number
//   char     delimiter[2] = {'w', 'r'}
//
// The backend unpacks payload with Python struct format:
//   '<B' + 'H' * (1 + numNodes) + 'I'
// where '<' is little-endian. ESP32-S3 is also little-endian.
static constexpr size_t WIRESENS_SEND_ID_SIZE = sizeof(uint8_t);
static constexpr size_t WIRESENS_START_IDX_SIZE = sizeof(uint16_t);
static constexpr size_t WIRESENS_READING_SIZE = sizeof(uint16_t);
static constexpr size_t WIRESENS_PACKET_NUMBER_SIZE = sizeof(uint32_t);
static constexpr size_t WIRESENS_DELIMITER_SIZE = 2;

static constexpr size_t WIRESENS_FRAME_NODE_CNT = NUM_FSR_DRIVES * NUM_FSR_SENSES;
static constexpr size_t WIRESENS_PACKET_BUF_SIZE =
    WIRESENS_SEND_ID_SIZE + WIRESENS_START_IDX_SIZE +
    WIRESENS_READING_SIZE * FSR_WIRESENS_MAX_NODES_PER_PACKET +
    WIRESENS_PACKET_NUMBER_SIZE + WIRESENS_DELIMITER_SIZE;

static uint8_t tx_packet_buf[WIRESENS_PACKET_BUF_SIZE];
static fsr_wiresens_cfg_t wiresens_cfg;
static uint32_t packet_number = 0;

fsr_wiresens_cfg_t fsr_wiresens_default_cfg(uart_port_t uart_num,
                                            gpio_num_t tx_gpio,
                                            gpio_num_t rx_gpio) {
    return (fsr_wiresens_cfg_t){
        .uart_num = uart_num,
        .tx_gpio = tx_gpio,
        .rx_gpio = rx_gpio,
        .baud_rate = FSR_WIRESENS_DEFAULT_BAUD_RATE,
        .sensor_id = 1,
        .nodes_per_packet = FSR_WIRESENS_DEFAULT_NODES_PER_PACKET,
        .tx_buffer_size = WIRESENS_PACKET_BUF_SIZE * 2,
    };
}

size_t fsr_wiresens_packet_payload_size(uint16_t nodes_per_packet) {
    return WIRESENS_SEND_ID_SIZE + WIRESENS_START_IDX_SIZE +
           WIRESENS_READING_SIZE * nodes_per_packet + WIRESENS_PACKET_NUMBER_SIZE;
}

size_t fsr_wiresens_packet_serial_size(uint16_t nodes_per_packet) {
    return fsr_wiresens_packet_payload_size(nodes_per_packet) + WIRESENS_DELIMITER_SIZE;
}

esp_err_t fsr_wiresens_pack_packet(uint8_t *dst,
                                   size_t dst_size,
                                   uint8_t sensor_id,
                                   uint16_t start_idx,
                                   const uint16_t *readings,
                                   uint16_t nodes_per_packet,
                                   uint32_t packet_number) {
    if (dst == nullptr || readings == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    if (nodes_per_packet == 0 || nodes_per_packet > FSR_WIRESENS_MAX_NODES_PER_PACKET) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t packet_size = fsr_wiresens_packet_serial_size(nodes_per_packet);
    if (dst_size < packet_size) {
        return ESP_ERR_INVALID_SIZE;
    }

    size_t off = 0;
    dst[off++] = sensor_id;

    // ESP32-S3 and the WiReSens packet format are both little-endian.
    // Copy fields individually instead of copying a struct to avoid padding.
    memcpy(&dst[off], &start_idx, sizeof(start_idx));
    off += 2;

    size_t readings_size = sizeof(readings[0]) * nodes_per_packet;
    memcpy(&dst[off], readings, readings_size);
    off += readings_size;

    memcpy(&dst[off], &packet_number, sizeof(packet_number));
    off += 4;

    dst[off++] = 'w';
    dst[off++] = 'r';

    return ESP_OK;
}

void fsr_wiresens_init(fsr_wiresens_cfg_t cfg) {
    if (cfg.baud_rate == 0) {
        cfg.baud_rate = FSR_WIRESENS_DEFAULT_BAUD_RATE;
    }
    if (cfg.nodes_per_packet == 0) {
        cfg.nodes_per_packet = FSR_WIRESENS_DEFAULT_NODES_PER_PACKET;
    }
    if (cfg.tx_buffer_size == 0) {
        cfg.tx_buffer_size = WIRESENS_PACKET_BUF_SIZE * 2;
    }

    ESP_ERROR_CHECK(cfg.nodes_per_packet <= FSR_WIRESENS_MAX_NODES_PER_PACKET ? ESP_OK : ESP_ERR_INVALID_ARG);
    ESP_ERROR_CHECK(WIRESENS_FRAME_NODE_CNT % cfg.nodes_per_packet == 0 ? ESP_OK : ESP_ERR_INVALID_ARG);

    uart_config_t uart_cfg = {
        .baud_rate = cfg.baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(cfg.uart_num, 256, cfg.tx_buffer_size, 0, nullptr, 0));
    ESP_ERROR_CHECK(uart_param_config(cfg.uart_num, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(cfg.uart_num,
                                 cfg.tx_gpio,
                                 cfg.rx_gpio,
                                 UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));

    wiresens_cfg = cfg;
    packet_number = 0;
}

esp_err_t fsr_wiresens_send_frame(const uint16_t *frame) {
    if (frame == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t nodes_per_packet = wiresens_cfg.nodes_per_packet;
    if (nodes_per_packet == 0 || WIRESENS_FRAME_NODE_CNT % nodes_per_packet != 0) {
        // make sure that every packet sends the same amount of node
        return ESP_ERR_INVALID_STATE;
    }

    size_t packet_size = fsr_wiresens_packet_serial_size(nodes_per_packet);
    for (uint16_t start_idx = 0; start_idx < WIRESENS_FRAME_NODE_CNT; start_idx += nodes_per_packet) {
        ESP_RETURN_ON_ERROR(fsr_wiresens_pack_packet(tx_packet_buf,
                                                     sizeof(tx_packet_buf),
                                                     wiresens_cfg.sensor_id,
                                                     start_idx,
                                                     &frame[start_idx],
                                                     nodes_per_packet,
                                                     packet_number),
                            "wiresens",
                            "failed to pack WiReSens packet");

        int written = uart_write_bytes(wiresens_cfg.uart_num, tx_packet_buf, packet_size);
        if (written != (int)packet_size) {
            return ESP_FAIL;
        }

        packet_number++;
    }

    return ESP_OK;
}
