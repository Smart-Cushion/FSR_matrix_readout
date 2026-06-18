#pragma once
#include "hal/adc_types.h"
#include "soc/gpio_num.h"
#include <stddef.h>
#include <stdint.h>

static const gpio_num_t FSR_DRIVE_ADDR_GPIOS[] = {
    GPIO_NUM_2,  // DRIVE_A0
    GPIO_NUM_42, // DRIVE_A1
    GPIO_NUM_41, // DRIVE_A2
    GPIO_NUM_40, // DRIVE_A3
};

static constexpr size_t NUM_FSR_DRIVE_ADDR_GPIOS =
    sizeof(FSR_DRIVE_ADDR_GPIOS) / sizeof(FSR_DRIVE_ADDR_GPIOS[0]);

typedef struct {
    adc_unit_t unit;
    adc_channel_t channel;
} fsr_sense_adc_map_t;

#define FSR_SENSE_ADC_MAP_ITEMS(X)                                             \
    X(0, ADC_UNIT_1, ADC_CHANNEL_1)                                            \
    X(1, ADC_UNIT_1, ADC_CHANNEL_0)                                            \
    X(2, ADC_UNIT_1, ADC_CHANNEL_3)                                            \
    X(3, ADC_UNIT_1, ADC_CHANNEL_4)                                            \
    X(4, ADC_UNIT_1, ADC_CHANNEL_5)                                            \
    X(5, ADC_UNIT_1, ADC_CHANNEL_6)                                            \
    X(6, ADC_UNIT_2, ADC_CHANNEL_4)                                            \
    X(7, ADC_UNIT_2, ADC_CHANNEL_5)                                            \
    X(8, ADC_UNIT_2, ADC_CHANNEL_6)                                            \
    X(9, ADC_UNIT_2, ADC_CHANNEL_7)                                            \
    X(10, ADC_UNIT_1, ADC_CHANNEL_7)                                           \
    X(11, ADC_UNIT_1, ADC_CHANNEL_2)                                           \
    X(12, ADC_UNIT_1, ADC_CHANNEL_8)                                           \
    X(13, ADC_UNIT_1, ADC_CHANNEL_9)                                           \
    X(14, ADC_UNIT_2, ADC_CHANNEL_0)                                           \
    X(15, ADC_UNIT_2, ADC_CHANNEL_1)

#define FSR_SENSE_ADC_MAP_ENTRY(index, adc_unit, adc_channel)                  \
    [index] = {.unit = adc_unit, .channel = adc_channel},

static const fsr_sense_adc_map_t FSR_SENSE_ADC_MAP[] = {
    FSR_SENSE_ADC_MAP_ITEMS(FSR_SENSE_ADC_MAP_ENTRY)
};

#undef FSR_SENSE_ADC_MAP_ENTRY

#define FSR_SENSE_INDEX_ENTRY(index, adc_unit, adc_channel)                    \
    [adc_unit][adc_channel] = index,

// ESP-IDF builds with GNU C; range initializers let unmapped ADC channels use
// -1.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverride-init"
static const int8_t FSR_SENSE_INDEX_LUT[2][11] = {
    [0 ... 1] =
        {
            [0 ... 10] = -1,
        },
    FSR_SENSE_ADC_MAP_ITEMS(FSR_SENSE_INDEX_ENTRY)
};
#pragma GCC diagnostic pop

#undef FSR_SENSE_INDEX_ENTRY

// maps the FSR row/col being sensed to a particular ADC unit and channel

static constexpr uint8_t NUM_FSR_DRIVES = 16;
static constexpr uint8_t NUM_FSR_SENSES = 16;

static constexpr gpio_num_t UART0_TX_GPIO = GPIO_NUM_43;
static constexpr gpio_num_t UART0_RX_GPIO = GPIO_NUM_44;