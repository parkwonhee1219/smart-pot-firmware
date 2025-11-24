#include "cds_task.h"
#include <driver/adc.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_log.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "adc_shared.h"

static const char *TAG = "cds_task";

#define CDS_ADC_CHANNEL       ADC_CHANNEL_6  // GPIO34

// Constants based on the CDS circuit configuration
#define R1 10000.0   // Series resistor value in ohms (10k ohm)
#define Vin 3.3      // Supply voltage in volts


void cds_task(void *ep)
{
    uint16_t cds_ep_id = *((uint16_t *)ep);

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
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, CDS_ADC_CHANNEL, &chan_cfg));

    adc_cali_handle_t cali_handle;
    adc_cali_line_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_line_fitting(&cali_cfg, &cali_handle));

    while (true) {
        int raw = 0, mv = 0;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, CDS_ADC_CHANNEL, &raw));
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali_handle, raw, &mv));

        float voltage = (float)mv / 1000.0;
        float r_cds = voltage * R1 / (Vin - voltage);
        if (r_cds < 1.0) r_cds = 1;

        float lux = 3981071.0 * pow(r_cds, -1.4);
        cds_sensor_notification(cds_ep_id, lux, NULL);

        ESP_LOGI(TAG, "CDS's lux: %.2f lux", lux);
        vTaskDelay(pdMS_TO_TICKS(10000));
    }

    adc_oneshot_del_unit(adc_handle);
    adc_cali_delete_scheme_line_fitting(cali_handle);
    vTaskDelete(NULL);
}
