#pragma once
#include "hal/adc_types.h"
#include "soc/gpio_num.h"
#include "soc/soc_caps.h"
#include <stddef.h>
#include <stdint.h>

static const gpio_num_t FSR_DRIVE_ADDR_GPIOS[] = {
    GPIO_NUM_42, // DRIVE_A0
    GPIO_NUM_41, // DRIVE_A1
    GPIO_NUM_40, // DRIVE_A2
    GPIO_NUM_39, // DRIVE_A3
};

static constexpr size_t NUM_FSR_DRIVE_ADDR_GPIOS =
    sizeof(FSR_DRIVE_ADDR_GPIOS) / sizeof(FSR_DRIVE_ADDR_GPIOS[0]);

typedef struct {
    adc_unit_t unit;
    adc_channel_t channel;
} fsr_sense_adc_map_t;

// Single source of truth for the sense-to-ADC mapping. The list generates
// both the forward configuration table and the reverse DMA lookup table.
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

// ESP-IDF builds with GNU C; range initializers set unmapped ADC channels to
// -1, and the generated entries intentionally override the mapped channels.

// provided the adc unit and the adc channel, this LUT will return the
// corresponding physical sensor index.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverride-init"
static const int8_t
    FSR_SENSE_INDEX_LUT[SOC_ADC_PERIPH_NUM][SOC_ADC_MAX_CHANNEL_NUM] = {
        [0 ... SOC_ADC_PERIPH_NUM - 1] =
            {
                [0 ... SOC_ADC_MAX_CHANNEL_NUM - 1] = -1,
            },
        FSR_SENSE_ADC_MAP_ITEMS(FSR_SENSE_INDEX_ENTRY)
};
#pragma GCC diagnostic pop

#undef FSR_SENSE_INDEX_ENTRY

static constexpr uint8_t NUM_FSR_DRIVES = 16;
static constexpr uint8_t NUM_FSR_SENSES = 16;

// UART0 is reserved for WiReSens data; route the system console through USB
// Serial/JTAG so logs cannot corrupt the binary stream.
static constexpr gpio_num_t UART0_TX_GPIO = GPIO_NUM_43;
static constexpr gpio_num_t UART0_RX_GPIO = GPIO_NUM_44;
