// firebase.h
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"


#ifdef __cplusplus
extern "C" {
#endif

void set_google_dns(void);

// 큐 초기화 (app_main에서 한 번 호출)
void fb_queue_init(void);

// 센서에서 값 들어오면 이거 호출해서 큐에 넣기만
void fb_update(const char *key, float value);

// 큐에서 꺼내서 실제로 Firebase로 보내는 태스크
void firebase_control_task(void *pv);
void firebase_sensor_task(void *pv);


#ifdef __cplusplus
}
#endif
