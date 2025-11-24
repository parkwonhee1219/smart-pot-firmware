#pragma once

#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

void led_driver_init(void);
void led_driver_set_power(bool power);

#ifdef __cplusplus
}
#endif
