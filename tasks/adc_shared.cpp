#include "adc_shared.h"
#include <esp_adc/adc_oneshot.h>

adc_oneshot_unit_handle_t adc_handle;

void init_shared_adc() {
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc_handle));
}
