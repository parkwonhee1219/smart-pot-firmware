#pragma once

#include <stdint.h> 
#ifdef __cplusplus
extern "C" {
#endif

void cds_sensor_notification(uint16_t endpoint_id, float illuminance, void *user_data);

void cds_task(void *arg);

#ifdef __cplusplus
}
#endif
