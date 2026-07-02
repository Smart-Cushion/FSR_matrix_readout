#include "sense.h"
#include "decoder.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "esp_rom_sys.h"
#include "pin_defs.h"
#include "soc/soc_caps.h"
#include <stddef.h>
#include <stdint.h>

static constexpr uint8_t MAX_SUPERSAMPLE_CNT = 32;

// One oneshot handle per ADC peripheral present in FSR_SENSE_ADC_MAP. This
// board spreads the 16 sense columns across ADC1 (10 channels) and ADC2 (6).
static adc_oneshot_unit_handle_t adc_handles[SOC_ADC_PERIPH_NUM] = {};
static uint8_t used_supersample_cnt = 1;
static uint32_t settle_us = 0;

// Read the sense columns one channel at a time with adc_oneshot instead of the
// multi-channel continuous DMA path. The ESP32-S3 continuous driver only
// samples the first ~2 pattern channels reliably (esp-idf #10636), and its
// ADC2 DMA has a hardware errata (ADC-183), which left 15 of 16 columns empty.
// Oneshot drives each channel individually from the CPU, so both ADC units and
// every channel read back correctly. This matches the upstream WiReSens
// firmware, which samples each point with a per-point oneshot read.
void fsr_sense_init(fsr_sense_cfg_t cfg) {
    ESP_ERROR_CHECK(
        cfg.supersample_cnt > 0 && cfg.supersample_cnt <= MAX_SUPERSAMPLE_CNT
            ? ESP_OK
            : ESP_ERR_INVALID_ARG
    );
    ESP_ERROR_CHECK(cfg.sample_freq_hz > 0 ? ESP_OK : ESP_ERR_INVALID_ARG);

    used_supersample_cnt = cfg.supersample_cnt;

    // Preserve the analog settling budget the DMA path spent discarding scan
    // rounds: discard_rounds complete 16-sense scans at sample_freq_hz, applied
    // here as a fixed busy-wait after each decoder (drive row) switch.
    settle_us = (uint32_t)((uint64_t)cfg.discard_rounds * NUM_FSR_SENSES *
                           1000000ULL / cfg.sample_freq_hz);

    for (size_t i = 0; i < NUM_FSR_SENSES; i++) {
        adc_unit_t unit = FSR_SENSE_ADC_MAP[i].unit;

        if (adc_handles[unit] == nullptr) {
            adc_oneshot_unit_init_cfg_t unit_cfg = {
                .unit_id = unit,
            };
            ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc_handles[unit]));
        }

        adc_oneshot_chan_cfg_t chan_cfg = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = (adc_bitwidth_t)cfg.bit_width,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(
            adc_handles[unit], FSR_SENSE_ADC_MAP[i].channel, &chan_cfg
        ));
    }
}

void fsr_sense_read_frame(uint16_t *buf) {
    for (uint32_t drive = 0; drive < NUM_FSR_DRIVES; drive++) {
        fsr_drive_decoder_write(drive);

        if (settle_us > 0) {
            esp_rom_delay_us(settle_us);
        }

        for (uint8_t sense = 0; sense < NUM_FSR_SENSES; sense++) {
            adc_oneshot_unit_handle_t handle =
                adc_handles[FSR_SENSE_ADC_MAP[sense].unit];
            adc_channel_t channel = FSR_SENSE_ADC_MAP[sense].channel;

            uint32_t sum = 0;
            for (uint8_t s = 0; s < used_supersample_cnt; s++) {
                int raw = 0;
                ESP_ERROR_CHECK(adc_oneshot_read(handle, channel, &raw));
                sum += (uint32_t)raw;
            }

            buf[drive * NUM_FSR_SENSES + sense] =
                (uint16_t)(sum / used_supersample_cnt);
        }
    }
}
