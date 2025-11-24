#pragma once
#include <esp_adc/adc_oneshot.h>

extern adc_oneshot_unit_handle_t adc_handle;
void init_shared_adc();  // 공통 초기화 함수
