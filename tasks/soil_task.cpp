#include "soil_task.h"
#include <esp_log.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <hal/adc_types.h>
#include <esp_matter.h>

#include "adc_shared.h"

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace chip::app::Clusters;

static const char *TAG = "soil_task";

#define SOIL_ADC_CHANNEL      ADC_CHANNEL_7  // GPIO35

void soil_moisture_task(void *ep)
{
    uint16_t soil_ep_id = *((uint16_t *)ep);

    // adc_oneshot_unit_handle_t adc_handle;
    // adc_oneshot_unit_init_cfg_t unit_cfg = {
    //     .unit_id = ADC_UNIT_1,
    //     .ulp_mode = ADC_ULP_MODE_DISABLE,
    // };
    // ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, SOIL_ADC_CHANNEL, &chan_cfg));

    adc_cali_handle_t cali_handle;
    adc_cali_line_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_line_fitting(&cali_cfg, &cali_handle));

    while (1) {
        int raw = 0, mv = 0;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, SOIL_ADC_CHANNEL, &raw));
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali_handle, raw, &mv));

        float percent = ((float)mv / 3300.0f) * 100.0f;
        float percent_cali = 100 - percent;
        humidity_sensor_notification(soil_ep_id, percent_cali, NULL);

        ESP_LOGI(TAG, "Soil Moisture Voltage: %d mV, Humidity: %.2f %%", mv, percent);
        vTaskDelay(pdMS_TO_TICKS(10000));
    }

    adc_oneshot_del_unit(adc_handle);
    adc_cali_delete_scheme_line_fitting(cali_handle);
    vTaskDelete(NULL);
}
