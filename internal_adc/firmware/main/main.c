#include "decoder.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "pin_defs.h"
#include "sense.h"
#include "soc/soc_caps.h"
#include "wiresens.h"
#include <stdint.h>

void app_main(void) {
    // while (true) {
    //     printf("hello\n");
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    // }
    static uint16_t frame_buf[NUM_FSR_DRIVES * NUM_FSR_SENSES];

    fsr_drive_decoder_init();
    fsr_sense_init((fsr_sense_cfg_t){
        .supersample_cnt = 8,
        .bit_width = SOC_ADC_DIGI_MAX_BITWIDTH,
        .sample_freq_hz = 80'000,
        .discard_rounds = 4,
    });

    fsr_wiresens_cfg_t wiresens_cfg =
        fsr_wiresens_default_cfg(UART_NUM_0, UART0_TX_GPIO, UART0_RX_GPIO);
    fsr_wiresens_init(wiresens_cfg);

    while (true) {
        fsr_sense_read_frame(frame_buf);
        ESP_ERROR_CHECK(fsr_wiresens_send_frame(frame_buf));
    }
}
