#include "driver/dedic_gpio.h"
#include "decoder.h"
#include "esp_err.h"
#include "pin_defs.h"
#include <stddef.h>

static dedic_gpio_bundle_handle_t bundle = nullptr;
static dedic_gpio_bundle_config_t bundle_config = {
    .gpio_array = FSR_DRIVE_ADDR_GPIOS,
    .array_size = NUM_FSR_DRIVE_ADDR_GPIOS,
    .flags = {
        .out_en = 1,
    }
};
static constexpr uint32_t BUNDLE_MASK = 0b1111;
// there are 4 address bits

void fsr_drive_decoder_init() {
    ESP_ERROR_CHECK(dedic_gpio_new_bundle(&bundle_config, &bundle));
}

void fsr_drive_decoder_write(uint32_t addr){
    dedic_gpio_bundle_write(bundle, BUNDLE_MASK, addr);
}