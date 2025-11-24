#pragma once

#include <stdint.h> 
#ifdef __cplusplus
extern "C" {
#endif

void humidity_sensor_notification(uint16_t endpoint_id, float humidity, void *user_data);

void soil_moisture_task(void *ep);

#ifdef __cplusplus
}
#endif
