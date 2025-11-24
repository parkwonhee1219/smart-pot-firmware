// firebase.cpp
#include "firebase.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"

#include "lwip/dns.h"
#include "lwip/ip_addr.h"

#include <string.h>

#define FB_KEY_MAX_LEN    16

#define FIREBASE_BASE_URL "https://smart-plant-app-1-default-rtdb.asia-southeast1.firebasedatabase.app/"

static const char *TAG = "FIREBASE";

typedef struct {
    char  key[FB_KEY_MAX_LEN];
    float value;
} fb_msg_t;

static QueueHandle_t s_sensor_queue = NULL;
static QueueHandle_t s_control_queue = NULL;

/* DNS 강제 설정 (Wi-Fi 붙은 뒤 한 번 호출) */
void set_google_dns(void)
{
    ip_addr_t dns;
    ipaddr_aton("8.8.8.8", &dns);
    dns_setserver(0, &dns);
    ESP_LOGI("DNS", "Set Google DNS: 8.8.8.8");
}

/* key 이름으로 bool/float 구분 */
static bool key_is_bool(const char *key)
{
    return (
        strcmp(key, "ledStatus") == 0 ||
        strcmp(key, "heatLedStatus") == 0 ||
        strcmp(key, "pumpStatus") == 0
    );
}

/* Firebase PATCH 전송 */
static esp_err_t firebase_send(const char *key, float value)
{
    char url[256];
    snprintf(url, sizeof(url), "%splant_data.json", FIREBASE_BASE_URL);

    char body[128];
    if (key_is_bool(key)) {
        snprintf(body, sizeof(body), "{\"%s\":%s}", key, (value == 1) ? "true" : "false");
    } else {
        snprintf(body, sizeof(body), "{\"%s\":%.2f}", key, value);
    }

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_PATCH,
        .timeout_ms = 4000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        ESP_LOGE(TAG, "init failed");
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, strlen(body));


    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    
    char resp[128] = {0};
    int rlen = esp_http_client_read_response(client, resp, sizeof(resp) - 1);

    if (err == ESP_OK) {
        ESP_LOGI(TAG,
                 "PATCH %s -> %s | status=%d, resp=%s",
                 key, body, status, (rlen > 0 ? resp : "<no body>"));
    } else {
        ESP_LOGE(TAG, "HTTP fail: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}


void fb_queue_init(void) {
    if (!s_sensor_queue) s_sensor_queue = xQueueCreate(20, sizeof(fb_msg_t));
    if (!s_control_queue) s_control_queue = xQueueCreate(10, sizeof(fb_msg_t));
}

void fb_update(const char *key, float value)
{

    if (!s_sensor_queue || !s_control_queue) {
        fb_queue_init();
    }

    fb_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    strncpy(msg.key, key, FB_KEY_MAX_LEN - 1);
    msg.value = value;
    
    // 제어용 키면 control queue로, 아니면 sensor queue로
    if (key_is_bool(key)) {
        xQueueSend(s_control_queue, &msg, 0);
    } else {
        xQueueSend(s_sensor_queue, &msg, 0);
    }
}

// 제어용 Firebase task (높은 우선순위)
void firebase_control_task(void *pv) {
    fb_msg_t msg;
    for (;;) {
        if (xQueueReceive(s_control_queue, &msg, portMAX_DELAY) == pdPASS) {
            firebase_send(msg.key, msg.value);
        }
    }
}

// 센서용 Firebase task (낮은 우선순위)
void firebase_sensor_task(void *pv) {
    fb_msg_t msg;
    for (;;) {
        if (xQueueReceive(s_sensor_queue, &msg, portMAX_DELAY) == pdPASS) {
            firebase_send(msg.key, msg.value);
        }
    }
}
