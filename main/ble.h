/* wifi.h | Wi-Fi Component Header (Including mDNS & NTP)

   This  code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/


#ifndef _BLE_H_
#define _BLE_H_

#include "esp_gap_ble_api.h"

void (* scan_result_callback)(struct ble_scan_result_evt_param sr);

void initialize_ble(void (*callback)(struct ble_scan_result_evt_param sr));

#endif
