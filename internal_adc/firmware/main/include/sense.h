#pragma once
#include <stdint.h>

typedef struct {
    uint8_t supersample_cnt;
    uint8_t bit_width;
    uint32_t sample_freq_hz;
    uint8_t discard_rounds;
} fsr_sense_cfg_t;

void fsr_sense_init(fsr_sense_cfg_t cfg);
void fsr_sense_read_frame(uint16_t *buf);
