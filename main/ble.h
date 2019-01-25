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

char* (* read_callback)();

void (* write_callback)(char *ssid, char *pswd);

void initialize_ble(void (*scan)(struct ble_scan_result_evt_param sr), char* (*read)(), void (*write)(char *ssid, char *pswd));

void close_gatt();

#endif
