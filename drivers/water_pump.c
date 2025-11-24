#include "water_pump.h"
#include <driver/gpio.h>
#include <esp_log.h>

static const char *TAG = "water_pump_driver";
static const gpio_num_t PUMP_GPIO = GPIO_NUM_5;
static bool current_power = false;

void water_pump_driver_init() {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PUMP_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(PUMP_GPIO, 1);
}

void water_pump_driver_set_power(bool power) {
    gpio_set_level(PUMP_GPIO, power ? 0 : 1);
    current_power = !power;
    ESP_LOGI(TAG, "water_pump is now %s", power ? "OFF" : "ON");
}
