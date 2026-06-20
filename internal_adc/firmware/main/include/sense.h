#pragma once
#include <stdint.h>

typedef struct {
    uint8_t supersample_cnt;
    uint8_t bit_width;
    uint32_t sample_freq_hz;
    uint8_t discard_rounds;
} fsr_sense_cfg_t;

void fsr_sense_init(fsr_sense_cfg_t cfg);

/**
 * Read one complete drive-major frame.
 *
 * The caller must provide space for every drive/sense pair. Element
 * [drive * NUM_FSR_SENSES + sense] receives the averaged ADC sample. This
 * function blocks until the complete frame has been acquired.
 */
void fsr_sense_read_frame(uint16_t *buf);
