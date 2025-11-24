/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <app/server/CommissioningWindowManager.h>
#include <app/server/Server.h>
#include <bsp/esp-bsp.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_matter.h>
#include <esp_matter_console.h>
#include <esp_matter_ota.h>
#include <nvs_flash.h>

#include <app_openthread_config.h>
#include <app_reset.h>
#include <common_macros.h>

// drivers implemented by this project
#include <drivers/led.h>
#include <drivers/water_pump.h>
#include <drivers/heat_led.h>

// tasks implemented by this project
#include <tasks/adc_shared.h>
#include <tasks/cds_task.h>
#include <tasks/dht11_task.h>
#include <tasks/soil_task.h>
#include <tasks/firebase.h>



static const char *TAG = "app_main";

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

#define DEFAULT_POWER false

// Store endpoint IDs for identifying devices in the attribute callback
uint16_t led_ep_id;
uint16_t water_pump_ep_id;
uint16_t heat_led_ep_id;
uint16_t dht11_ep_ids[2];


// Application cluster specification, 7.18.2.11. Temperature
// represents a temperature on the Celsius scale with a resolution of 0.01°C.
// temp = (temperature in °C) x 100
void temp_sensor_notification(uint16_t endpoint_id, float temp, void *user_data)
{
    // schedule the attribute update so that we can report it from matter thread
    chip::DeviceLayer::SystemLayer().ScheduleLambda([endpoint_id, temp]() {
        attribute_t * attribute = attribute::get(endpoint_id,
                                                 TemperatureMeasurement::Id,
                                                 TemperatureMeasurement::Attributes::MeasuredValue::Id);

        esp_matter_attr_val_t val = esp_matter_invalid(NULL);
        attribute::get_val(attribute, &val);
        val.val.i16 = static_cast<int16_t>(temp * 100);

        attribute::update(endpoint_id, TemperatureMeasurement::Id, TemperatureMeasurement::Attributes::MeasuredValue::Id, &val);
    });
    fb_update("temperature",temp);
}

// Application cluster specification, 2.6.4.1. MeasuredValue Attribute
// represents the humidity in percent.
// humidity = (humidity in %) x 100
void humidity_sensor_notification(uint16_t endpoint_id, float humidity, void *user_data)
{
    // schedule the attribute update so that we can report it from matter thread
    chip::DeviceLayer::SystemLayer().ScheduleLambda([endpoint_id, humidity]() {
        attribute_t * attribute = attribute::get(endpoint_id,
                                                 RelativeHumidityMeasurement::Id,
                                                 RelativeHumidityMeasurement::Attributes::MeasuredValue::Id);

        esp_matter_attr_val_t val = esp_matter_invalid(NULL);
        attribute::get_val(attribute, &val);
        val.val.u16 = static_cast<uint16_t>(humidity * 100);

        attribute::update(endpoint_id, RelativeHumidityMeasurement::Id, RelativeHumidityMeasurement::Attributes::MeasuredValue::Id, &val);
        
    });
    if (endpoint_id == dht11_ep_ids[1]) {
        fb_update("humidity", humidity);
    }
    else {
        fb_update("soilMoisture",humidity);
    }
    
}

// cds cluster specification
void cds_sensor_notification(uint16_t endpoint_id, float illuminance, void *user_data){
    chip::DeviceLayer::SystemLayer().ScheduleLambda([endpoint_id, illuminance](){
        attribute_t *attribute = attribute::get(endpoint_id, IlluminanceMeasurement::Id, IlluminanceMeasurement::Attributes::MeasuredValue::Id);
    esp_matter_attr_val_t val = esp_matter_invalid(NULL);
    attribute::get_val(attribute, &val);
    val.val.u16 = static_cast<uint16_t>(illuminance);

    attribute::update(endpoint_id, IlluminanceMeasurement::Id, IlluminanceMeasurement::Attributes::MeasuredValue::Id, &val);
    });
    fb_update("lightIntensity",illuminance);
};


static esp_err_t factory_reset_button_register()
{
    button_handle_t push_button;
    esp_err_t err = bsp_iot_button_create(&push_button, NULL, BSP_BUTTON_NUM);
    VerifyOrReturnError(err == ESP_OK, err);
    return app_reset_button_register(push_button);
}

static void open_commissioning_window_if_necessary()
{
    VerifyOrReturn(chip::Server::GetInstance().GetFabricTable().FabricCount() == 0);

    chip::CommissioningWindowManager & commissionMgr = chip::Server::GetInstance().GetCommissioningWindowManager();
    VerifyOrReturn(commissionMgr.IsCommissioningWindowOpen() == false);

    // After removing last fabric, this example does not remove the Wi-Fi credentials
    // and still has IP connectivity so, only advertising on DNS-SD.
    CHIP_ERROR err = commissionMgr.OpenBasicCommissioningWindow(chip::System::Clock::Seconds16(300),
                                    chip::CommissioningWindowAdvertisement::kDnssdOnly);
    if (err != CHIP_NO_ERROR)
    {
        ESP_LOGE(TAG, "Failed to open commissioning window, err:%" CHIP_ERROR_FORMAT, err.Format());
    }
}

static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
        ESP_LOGI(TAG, "Commissioning complete");
        set_google_dns();
        break;

    case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:
        ESP_LOGI(TAG, "Commissioning failed, fail safe timer expired");
        break;

    case chip::DeviceLayer::DeviceEventType::kFabricRemoved:
        ESP_LOGI(TAG, "Fabric removed successfully");
        open_commissioning_window_if_necessary();
        break;

    case chip::DeviceLayer::DeviceEventType::kBLEDeinitialized:
        ESP_LOGI(TAG, "BLE deinitialized and memory reclaimed");
        break;


    default:
        break;
    }
}

// This callback is invoked when clients interact with the Identify Cluster.
// In the callback implementation, an endpoint can identify itself. (e.g., by flashing an LED or light).
static esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id, uint8_t effect_id,
                                       uint8_t effect_variant, void *priv_data)
{
    ESP_LOGI(TAG, "Identification callback: type: %u, effect: %u, variant: %u", type, effect_id, effect_variant);
    return ESP_OK;
}

// This callback is called for every attribute update. The callback implementation shall
// handle the desired attributes and return an appropriate error code. If the attribute
// is not of your interest, please do not return an error code and strictly return ESP_OK.
static esp_err_t app_attribute_update_cb(attribute::callback_type_t type,
                                         uint16_t endpoint_id,
                                         uint32_t cluster_id,
                                         uint32_t attribute_id,
                                         esp_matter_attr_val_t *val,
                                         void *priv_data) {
    // if (type == PRE_UPDATE && cluster_id == OnOff::Id && attribute_id == OnOff::Attributes::OnOff::Id) {
    //     if (endpoint_id == led_ep_id) {
    //         led_driver_set_power(val->val.b);
    //         fb_update("ledStatus",val->val.b)
    //     } 
    //     else if(endpoint_id == heat_led_ep_id){
    //         heat_led_driver_set_power(val->val.b);
    //         fb_update("HeatledStatus",val->val.b)
    //     }
    //     else if (endpoint_id == water_pump_ep_id) {
    //         water_pump_driver_set_power(val->val.b);
    //         fb_update("pumpStatus",val->val.b)
    //     } else {
    //         ESP_LOGW(TAG, "Unknown endpoint ID %d for OnOff", endpoint_id);
    //     }
    // }
    if (type == PRE_UPDATE && cluster_id == OnOff::Id && attribute_id == OnOff::Attributes::OnOff::Id){

        if (endpoint_id == led_ep_id) {
            led_driver_set_power(val->val.b);
            fb_update("ledStatus", val->val.b ? 1:0);
        }
        else if (endpoint_id == heat_led_ep_id) {
            heat_led_driver_set_power(val->val.b);
            fb_update("heatLedStatus", val->val.b ? 1:0);
        }
        else if (endpoint_id == water_pump_ep_id) {
            water_pump_driver_set_power(val->val.b);
            fb_update("pumpStatus", val->val.b ? 1:0);
        }
        else {
            ESP_LOGW(TAG, "Unknown endpoint ID %d for OnOff", endpoint_id);
        }
    }

    return ESP_OK;
}



extern "C" void app_main()
{
    /* Initialize the ESP NVS layer */
    nvs_flash_init();

    /* Initialize queue*/
    fb_queue_init();

    /* Initialize push button on the dev-kit to reset the device */
    esp_err_t err = factory_reset_button_register();
    ABORT_APP_ON_FAILURE(ESP_OK == err, ESP_LOGE(TAG, "Failed to initialize reset button, err:%d", err));

    /* Initialize driver */
    led_driver_init();
    heat_led_driver_init();
    water_pump_driver_init();

    /* Initialize shared adc handle */
    init_shared_adc();

    /* Create a Matter node and add the mandatory Root Node device type on endpoint 0 */
    node::config_t node_config;
    node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);
    ABORT_APP_ON_FAILURE(node != nullptr, ESP_LOGE(TAG, "Failed to create Matter node"));

    // add led device
    on_off_light::config_t light_config;
    light_config.on_off.on_off = DEFAULT_POWER;
    light_config.on_off.feature_flags = 0;
    endpoint_t *led_ep = on_off_light::create(node, &light_config, ENDPOINT_FLAG_NONE, nullptr);
    ABORT_APP_ON_FAILURE(led_ep != nullptr, ESP_LOGE(TAG, "Failed to create led endpoint"));

    // add heat led device
    on_off_light::config_t heat_light_config;
    light_config.on_off.on_off = DEFAULT_POWER;
    light_config.on_off.feature_flags = 0;
    endpoint_t *heat_led_ep = on_off_light::create(node, &heat_light_config, ENDPOINT_FLAG_NONE, nullptr);
    ABORT_APP_ON_FAILURE(heat_led_ep != nullptr, ESP_LOGE(TAG, "Failed to create heat led endpoint"));

    // add water_pump device
    on_off_light::config_t water_pump_config;
    water_pump_config.on_off.on_off = DEFAULT_POWER;
    water_pump_config.on_off.feature_flags = 0;
    endpoint_t *water_pump_ep = on_off_light::create(node, &water_pump_config, ENDPOINT_FLAG_NONE, nullptr);
    ABORT_APP_ON_FAILURE(water_pump_ep != nullptr, ESP_LOGE(TAG, "Failed to create water_pump endpoint"));

    // add cds sensor device
    light_sensor::config_t cds_config;
    endpoint_t *cds_ep = light_sensor::create(node, &cds_config, ENDPOINT_FLAG_NONE, NULL);
    ABORT_APP_ON_FAILURE(cds_ep != nullptr, ESP_LOGE(TAG, "Failed to create cds sensor endpoint"));

    // add dht11 sensor device
    temperature_sensor::config_t dht11_temp_config;
    endpoint_t * dht11_temp_ep = temperature_sensor::create(node, &dht11_temp_config, ENDPOINT_FLAG_NONE, NULL);
    ABORT_APP_ON_FAILURE(dht11_temp_ep != nullptr, ESP_LOGE(TAG, "Failed to create temperature_sensor endpoint"));

    humidity_sensor::config_t dht11_humidity_config;
    endpoint_t * dht11_humidity_ep = humidity_sensor::create(node, &dht11_humidity_config, ENDPOINT_FLAG_NONE, NULL);
    ABORT_APP_ON_FAILURE(dht11_humidity_ep != nullptr, ESP_LOGE(TAG, "Failed to create humidity_sensor endpoint"));

    // add soil moisture sensor device
    humidity_sensor::config_t soil_humidity_config;
    endpoint_t * soil_humidity_ep = humidity_sensor::create(node, &soil_humidity_config, ENDPOINT_FLAG_NONE, NULL);
    ABORT_APP_ON_FAILURE(soil_humidity_ep != nullptr, ESP_LOGE(TAG, "Failed to create humidity_sensor endpoint"));


#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
    /* Set OpenThread platform config */
    esp_openthread_platform_config_t config = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
    };
    set_openthread_platform_config(&config);
#endif

    // led endpoint id
    led_ep_id = endpoint::get_id(led_ep);

    // heat led endpoint id
    heat_led_ep_id = endpoint::get_id(heat_led_ep);

    // water pump endpoint id
    water_pump_ep_id = endpoint::get_id(water_pump_ep);

    // cds endpoint id
    uint16_t cds_ep_id = endpoint::get_id(cds_ep);

    // dht11 endpoint id
    dht11_ep_ids[0] = endpoint::get_id(dht11_temp_ep);
    dht11_ep_ids[1] = endpoint::get_id(dht11_humidity_ep);

    // soil moisture sensor endpoint id
    uint16_t soil_ep_id = endpoint::get_id(soil_humidity_ep);

    /* Matter start */
    err = esp_matter::start(app_event_cb);
    ABORT_APP_ON_FAILURE(err == ESP_OK, ESP_LOGE(TAG, "Failed to start Matter, err:%d", err));
    
    xTaskCreate(firebase_control_task, "fb_control", 4096, NULL, 6, NULL); 
    xTaskCreate(dht11_task, "dht11_task", 4096, dht11_ep_ids, 5, NULL);
    xTaskCreate(soil_moisture_task, "soil_moisture_task", 4096, &soil_ep_id, 5, NULL);
    xTaskCreate(cds_task, "cds task", 4096, &cds_ep_id, 5, NULL);
    xTaskCreate(firebase_sensor_task,  "fb_sensor",  4096, NULL, 4, NULL); 
}
