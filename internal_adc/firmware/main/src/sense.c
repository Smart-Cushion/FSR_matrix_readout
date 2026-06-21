#include "sense.h"
#include "decoder.h"
#include "esp_adc/adc_continuous.h"
#include "esp_err.h"
#include "pin_defs.h"
#include "soc/soc_caps.h"
#include <stddef.h>
#include <stdint.h>
#include <sys/param.h>

static constexpr uint8_t MAX_SUPERSAMPLE_CNT = 32;
static constexpr size_t MAX_FRAME_SAMPLE_CNT =
    NUM_FSR_SENSES * MAX_SUPERSAMPLE_CNT;
static constexpr size_t MAX_FRAME_BUF_SIZE =
    MAX_FRAME_SAMPLE_CNT * SOC_ADC_DIGI_RESULT_BYTES;
static constexpr size_t MAX_STORE_BUF_SIZE = MAX_FRAME_BUF_SIZE * 2;
static constexpr uint8_t MAX_DRAIN_READ_CNT = 4;
static constexpr uint32_t READ_TIMEOUT_MULTIPLIER = 10;
static constexpr uint32_t MIN_READ_TIMEOUT_MS = 50;
static uint8_t adc_dma_buf[MAX_STORE_BUF_SIZE];
static size_t used_frame_sample_cnt = 0;
static size_t used_frame_buf_size = 0;
static size_t discard_buf_size = 0;
static uint32_t read_timeout_ms = 0;

static adc_continuous_handle_t handle = nullptr;

static void fsr_sense_drain_pool() {
    uint32_t read_size = 0;

    // attempt to drain the DMA buffer, but return after a maxium number of
    // reads to avoid blocking indefinitely if the driver fails
    for (uint8_t i = 0; i < MAX_DRAIN_READ_CNT; i++) {
        if (adc_continuous_read(
                handle, adc_dma_buf, MAX_STORE_BUF_SIZE, &read_size, 0
            ) != ESP_OK) {
            break;
        }
    }
}

static void fsr_sense_discard_samples() {
    uint32_t read_size = 0;
    size_t discarded_size = 0;

    while (discarded_size < discard_buf_size) {
        size_t remaining_size = discard_buf_size - discarded_size;
        uint32_t read_buf_size = remaining_size < MAX_STORE_BUF_SIZE
                                     ? remaining_size
                                     : MAX_STORE_BUF_SIZE;

        ESP_ERROR_CHECK(adc_continuous_read(
            handle, adc_dma_buf, read_buf_size, &read_size, read_timeout_ms
        ));
        discarded_size += read_size;
    }
}

void fsr_sense_init(fsr_sense_cfg_t cfg) {
    ESP_ERROR_CHECK(
        cfg.supersample_cnt > 0 && cfg.supersample_cnt <= MAX_SUPERSAMPLE_CNT
            ? ESP_OK
            : ESP_ERR_INVALID_ARG
    );
    ESP_ERROR_CHECK(cfg.sample_freq_hz > 0 ? ESP_OK : ESP_ERR_INVALID_ARG);

    used_frame_sample_cnt = cfg.supersample_cnt * NUM_FSR_SENSES;
    used_frame_buf_size = used_frame_sample_cnt * SOC_ADC_DIGI_RESULT_BYTES;
    discard_buf_size =
        cfg.discard_rounds * NUM_FSR_SENSES * SOC_ADC_DIGI_RESULT_BYTES;

    // Multiply the theoretical frame time by a configurable margin, round up
    // to a whole millisecond, and allow at least several FreeRTOS ticks.
    uint32_t theoretical_timeout_ms =
        (used_frame_sample_cnt * 1000 * READ_TIMEOUT_MULTIPLIER +
         cfg.sample_freq_hz - 1) /
        cfg.sample_freq_hz;
    read_timeout_ms = MAX(theoretical_timeout_ms, MIN_READ_TIMEOUT_MS);

    adc_continuous_handle_cfg_t adc_handle_cfg = {
        .max_store_buf_size = MAX_STORE_BUF_SIZE,
        .conv_frame_size = used_frame_buf_size,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_handle_cfg, &handle));

    adc_digi_pattern_config_t pattern_cfg[NUM_FSR_SENSES] = {};
    for (size_t i = 0; i < NUM_FSR_SENSES; i++) {
        pattern_cfg[i].atten = ADC_ATTEN_DB_12;
        pattern_cfg[i].channel = FSR_SENSE_ADC_MAP[i].channel;
        pattern_cfg[i].unit = FSR_SENSE_ADC_MAP[i].unit;
        pattern_cfg[i].bit_width = cfg.bit_width;
    }

    // This board uses ADC2 channels. On ESP32-S3, forcing ADC2 continuous DMA
    // with CONFIG_ADC_CONTINUOUS_FORCE_USE_ADC2_ON_C3_S3 only bypasses the
    // driver's ADC1-only guard; it does not resolve the ADC2 hardware errata.
    adc_continuous_config_t adc_cfg = {
        .pattern_num = NUM_FSR_SENSES,
        .adc_pattern = pattern_cfg,
        .sample_freq_hz = cfg.sample_freq_hz,
        .conv_mode = ADC_CONV_ALTER_UNIT,
    };
    ESP_ERROR_CHECK(adc_continuous_config(handle, &adc_cfg));
}

void fsr_sense_read_frame(uint16_t *buf) {
    static adc_continuous_data_t parsed_data[MAX_FRAME_SAMPLE_CNT] = {};
    // This maximum-sized buffer exceeds the main task stack, so it is static.
    // The shared buffer also makes this module non-reentrant.

    ESP_ERROR_CHECK(adc_continuous_start(handle));

    for (uint32_t drive = 0; drive < NUM_FSR_DRIVES; drive++) {
        uint32_t sample_sum[NUM_FSR_SENSES] = {};
        uint16_t sample_cnt[NUM_FSR_SENSES] = {};
        uint32_t read_size = 0;
        uint32_t parsed_sample_cnt = 0;

        // Remove queued samples from the previous drive, switch the decoder,
        // then discard complete scan rounds captured while the analog path
        // settles.
        fsr_sense_drain_pool();
        fsr_drive_decoder_write(drive);
        fsr_sense_discard_samples();

        ESP_ERROR_CHECK(adc_continuous_read(
            handle, adc_dma_buf, used_frame_buf_size, &read_size,
            read_timeout_ms
        ));
        ESP_ERROR_CHECK(adc_continuous_parse_data(
            handle, adc_dma_buf, read_size, parsed_data, &parsed_sample_cnt
        ));

        for (uint32_t i = 0; i < parsed_sample_cnt; i++) {
            if (!parsed_data[i].valid) {
                continue;
            }

            int8_t sense = FSR_SENSE_INDEX_LUT[parsed_data[i].unit]
                                              [parsed_data[i].channel];
            if (sense < 0) {
                continue;
            }

            sample_sum[sense] += parsed_data[i].raw_data;
            sample_cnt[sense]++;
        }

        for (uint8_t sense = 0; sense < NUM_FSR_SENSES; sense++) {
            uint32_t sum = sample_sum[sense];
            uint16_t cnt = sample_cnt[sense];
            buf[drive * NUM_FSR_SENSES + sense] = cnt > 0 ? sum / cnt : 0;
        }
    }

    ESP_ERROR_CHECK(adc_continuous_stop(handle));
}
