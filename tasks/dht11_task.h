#pragma once

#include <stdint.h> 
#ifdef __cplusplus
extern "C" {
#endif

void temp_sensor_notification(uint16_t endpoint_id, float temp, void *user_data);
void humidity_sensor_notification(uint16_t endpoint_id, float humidity, void *user_data);

void dht11_task(void *ep_ids);

#ifdef __cplusplus
}
#endif
