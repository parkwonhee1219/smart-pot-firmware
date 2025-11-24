#include "heat_led.h"
#include <driver/gpio.h>
#include <esp_log.h>

static const char *TAG = "heat_led_driver";
static const gpio_num_t HEAT_GPIO = GPIO_NUM_19;
static bool current_power = false;

void heat_led_driver_init() {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << HEAT_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(HEAT_GPIO, 0);
}

void heat_led_driver_set_power(bool power) {
    gpio_set_level(HEAT_GPIO, power ? 1 : 0);
    current_power = power;
    ESP_LOGI(TAG, "HEAT LED is now %s", power ? "ON" : "OFF");
}
