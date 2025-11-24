
#include "dht11_task.h"
#include <esp_log.h>
#include <drivers/dht.h>
#include <esp_matter.h>

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace chip::app::Clusters;

static const char *TAG = "dht11_task";

static const gpio_num_t DHT_GPIO = GPIO_NUM_18;

void dht11_task(void *ep_ids)
{
    uint16_t temp_ep_id = ((uint16_t *)ep_ids)[0];
    uint16_t humi_ep_id = ((uint16_t *)ep_ids)[1];

    while (1) {
        int16_t temp = 0, humi = 0;
        if (dht_read_data(DHT_TYPE_DHT11, DHT_GPIO, &humi, &temp) == ESP_OK) {
            ESP_LOGI(TAG, "DHT11 Read Success: Temp=%d, Humi=%d", temp, humi);
            temp_sensor_notification(temp_ep_id, temp / 10.0, NULL);
            humidity_sensor_notification(humi_ep_id, humi / 10.0, NULL);
        } else {
            ESP_LOGE(TAG, "DHT11 Read Failed");
        }
        vTaskDelay(pdMS_TO_TICKS(20000));
    }
}
