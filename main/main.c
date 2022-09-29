/* main.c | ESP32 Gateway Main

   This  code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "powerblade.h"
#include "wifi.h"
#include "ble.h"

#define TAG "GATEWAY"

static powerblade_item items[100];
static int items_start = 0, items_end = 0;

/* Strings */
static char body[32768], entry[512];

static void http_post_task() {
    while (1) {
        vTaskDelay( 5000 / portTICK_PERIOD_MS);
        if (items_end - items_start) {
            /* If new data is available, set up a POST request */
            int n = items_start;
            for (body[0] = 0; n != items_end; n = (n + 1) % (sizeof(items) / sizeof(powerblade_item))) {
                powerblade_item_to_influx_string(&items[n], entry);
                //sprintf(body, "%s %s", body, entry);
            }
            ESP_LOGI(TAG, "HTTP POST %d Items to Influx", (n-items_start+100)%100);
            if (http_post_to_influx(body) == 204) {
                items_start = n; // update cursor on influx success (status == 204)
            }
        }
    }
}

static void ble_scan_result_callback(struct ble_scan_result_evt_param sr) {
    /* If ad is PowerBlade data, populate current item with parsed values & point to next item */
    if (parse_powerblade_data(sr.bda, sr.ble_adv, sr.adv_data_len, &items[items_end])) {
        items_end = (items_end + 1) % (sizeof(items) / sizeof(powerblade_item));
    }
}

void app_main() {
    initialize_nvs();                         // Initialize non-volatile storage
    initialize_wifi();                        // Set up Wi-Fi & connect to network, if specified
    initialize_server();                      // Serve config page on the local network
    initialize_mdns("ESP32");                 // Advertise over mDNS / ZeroConf / Bonjour
    initialize_ble(ble_scan_result_callback); // Set up BLE & register scan result callback
    
    xTaskCreate(&http_post_task, "http_post_task", 32*configMINIMAL_STACK_SIZE, NULL, 10, NULL); // Initiate looped HTTP POST task
}
