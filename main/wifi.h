/* wifi.h | Wi-Fi Component Header (Including mDNS & NTP)

   This  code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/


#ifndef _WIFI_H_
#define _WIFI_H_

void initialize_nvs();

void initialize_wifi();

void initialize_sntp();

void initialize_mdns(char *mdns_name);

void initialize_server();

int http_post(char *url, char *body);

int http_post_to_influx(char *body);

#endif
