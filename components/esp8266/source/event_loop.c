// Copyright 2018 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_event_loop.h"

#include "esp_log.h"
#include "sdkconfig.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

static const char* TAG = "event";
static bool s_event_init_flag = false;
static void *s_event_queue = NULL;
static system_event_cb_t s_event_handler_cb = NULL;
static void *s_event_ctx = NULL;

static esp_err_t esp_event_post_to_user(system_event_t *event)
{
    if (s_event_handler_cb) {
        return (*s_event_handler_cb)(s_event_ctx, event);
    }
    return ESP_OK;
}

static void esp_event_loop_task(void *pvParameters)
{
    while (1) {
        system_event_t evt;
        if (xQueueReceive(s_event_queue, &evt, portMAX_DELAY) == pdPASS) {
            esp_err_t ret = esp_event_process_default(&evt);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "default event handler failed!");
            }
            ret = esp_event_post_to_user(&evt);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "post event to user fail!");
            }
        }
    }
}

system_event_cb_t esp_event_loop_set_cb(system_event_cb_t cb, void *ctx)
{
    system_event_cb_t old_cb = s_event_handler_cb;
    s_event_handler_cb = cb;
    s_event_ctx = ctx;
    return old_cb;
}

esp_err_t esp_event_send(system_event_t *event)
{
    if (s_event_queue == NULL) {
        ESP_LOGE(TAG, "Event loop not initialized via esp_event_loop_init, but esp_event_send called");
        return ESP_ERR_INVALID_STATE;
    }

    int ret = xQueueGenericSend(s_event_queue, event, 0, queueSEND_TO_BACK);
    if (ret != pdPASS) {
        if (event) {
            ESP_LOGE(TAG, "e=%d f", event->event_id);
        } else {
            ESP_LOGE(TAG, "e null");
        }
        return ESP_FAIL;
    }
    return ESP_OK;
}

QueueHandle_t esp_event_loop_get_queue(void)
{
    return s_event_queue;
}

esp_err_t esp_event_loop_init(system_event_cb_t cb, void *ctx)
{
    if (s_event_init_flag) {
        return ESP_FAIL;
    }
    s_event_handler_cb = cb;
    s_event_ctx = ctx;
    s_event_queue = xQueueCreate(32, sizeof(system_event_t));
    if(s_event_queue == NULL)
        return ESP_ERR_NO_MEM;
    if(xTaskCreate(esp_event_loop_task, "esp_event_loop_task", EVENT_LOOP_STACKSIZE, NULL, configMAX_PRIORITIES - 5, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    s_event_handler_cb = cb;
    s_event_ctx = ctx;
    s_event_init_flag = true;
    return ESP_OK;
}

esp_err_t esp_event_loop_create_default(void)
{
    return ESP_OK;
}