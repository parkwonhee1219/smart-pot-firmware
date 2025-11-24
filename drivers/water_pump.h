#pragma once

#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

void water_pump_driver_init(void);
void water_pump_driver_set_power(bool power);

#ifdef __cplusplus
}
#endif
