/* wifi.c | Wi-Fi Component Implementation (Including mDNS & NTP)

   This  code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/


#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_http_client.h"
#include "lwip/apps/sntp.h"
#include "lwip/api.h"
#include "nvs_flash.h"
#include "esp_coexist.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "mdns.h"
#include "www.h"
#include "wifi.h"

#define TAG "COMPONENT:WIFI"

/* Event group to signal when Wi-Fi is connected */
static EventGroupHandle_t wifi_group;

/* Values for Influx Endpoint */
static char host[128] = CONFIG_HOST, datb[32] = CONFIG_DATB, user[32] = CONFIG_USER, pswd[32] = CONFIG_PSWD;

static char recv_buf[64], url[256];
static char ip[16] = "";
static char ssid[32] = "";
static wifi_config_t sta_config = { .sta={.ssid=CONFIG_WIFI_SSID,.password=CONFIG_WIFI_PASSWORD} };

nvs_handle storage;

/* Callback functions assigned in initialize_wifi() */
void (* connect_callback)();
void (* disconnect_callback)();

static void wifi_handler(void *ctx, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
            nvs_open("storage", NVS_READWRITE, &storage);
            size_t ssid_len = 32, pswd_len = 64;
            char ssid_val[32] = "", pswd_val[64] = "";
            nvs_get_str(storage, "ssid", ssid_val, &ssid_len);
            nvs_get_str(storage, "pswd", pswd_val, &pswd_len);
            nvs_close(storage);
            set_ssid( ssid_val, pswd_val );
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
            xEventGroupSetBits(wifi_group, BIT0);
            nvs_open("storage", NVS_READWRITE, &storage);
            nvs_set_str(storage, "ssid", (char*)sta_config.sta.ssid);
            nvs_set_str(storage, "pswd", (char*)sta_config.sta.password);
            nvs_close(storage);
            strcpy(ssid, (char*)sta_config.sta.ssid);
            ESP_LOGI(TAG, "Connected to network: %s", ssid);
            // http_get_ip();
            connect_callback();       // Run main app's callback
            initialize_server();      // Serve config page on the local network
            initialize_mdns("ESP32"); // Advertise over mDNS / ZeroConf / Bonjour
            initialize_sntp();        // Set time
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
            ESP_LOGI(TAG, "Disconnected from network");
            esp_wifi_connect();
            xEventGroupClearBits(wifi_group, BIT0);
        // } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        //     uint16_t apCount = 0;
        //     esp_wifi_scan_get_ap_num(&apCount);
        //     if (apCount == 0) {
        //         ESP_LOGI(TAG, "No AP found");
        //         return;
        //     }
        //     wifi_ap_record_t *ap_list = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * apCount);
        //     if (!ap_list) {
        //         ESP_LOGE(TAG, "malloc error, ap_list is NULL");
        //         return;
        //     }
        //     ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&apCount, ap_list));
        //     esp_wifi_scan_stop();
        //     free(ap_list);
        // }
    }
}

void initialize_nvs() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    nvs_open("storage", NVS_READWRITE, &storage);
    size_t host_len = 128, datb_len = 32, user_len = 32, pswd_len = 32;
    nvs_get_str(storage, "iflx_host", host, &host_len);
    nvs_get_str(storage, "iflx_datb", datb, &datb_len);
    nvs_get_str(storage, "iflx_user", user, &user_len);
    nvs_get_str(storage, "iflx_pswd", pswd, &pswd_len);
    nvs_close(storage);
    ESP_ERROR_CHECK(ret);
}

void initialize_wifi(void (* connect)(), void (* disconnect)()) {
    esp_log_level_set("wifi", ESP_LOG_NONE);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    wifi_group = xEventGroupCreate();
    connect_callback = connect;
    disconnect_callback = disconnect;
    ESP_ERROR_CHECK( esp_netif_init() );
    ESP_ERROR_CHECK( esp_event_loop_create_default() );
    ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_handler, NULL) );
    ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_handler, NULL) );
    esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    xEventGroupWaitBits(wifi_group, BIT0, false, true, portMAX_DELAY);
}

void initialize_mdns(char *mdns_name) {
    ESP_ERROR_CHECK( mdns_init() );
    mdns_hostname_set(mdns_name);
    mdns_instance_name_set(mdns_name);
    mdns_service_add(mdns_name, "_http", "_tcp", 80, NULL, 0);
}

void initialize_sntp() {
    if ( sntp_enabled() ) {
        sntp_stop();
    }
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}

void http_get_ip(char *url, char *body) {
    esp_http_client_config_t config = { .url = "http://api.ipify.org" };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!esp_http_client_perform(client)) {
        esp_http_client_read(client, ip, sizeof(ip)-1);
        ESP_LOGI(TAG,"PUBLIC IP: %s",ip);
    }
    esp_http_client_cleanup(client);
}

int http_post(char *url, char *body) {
    uint8_t status = 0;
    esp_http_client_config_t config = { .url = url };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(client, body, strlen(body));
    esp_coex_preference_set(ESP_COEX_PREFER_WIFI);
    esp_err_t error = esp_http_client_perform(client);
    esp_coex_preference_set(ESP_COEX_PREFER_BT);
    if (error) {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(error));
    } else {
        status = esp_http_client_get_status_code(client);
        if (status != 204) {
            ESP_LOGW(TAG, "HTTP POST Status = %d, Response:", esp_http_client_get_status_code(client));
            while (esp_http_client_read(client, recv_buf, sizeof(recv_buf)-1) > 0 || (putchar('\n') && 0) ) {
                printf(recv_buf);
            }
        }
    }
    esp_http_client_cleanup(client);
    return status;
}

int http_post_to_influx(char *body) {
    if (user[0] && pswd[0]) {
        sprintf(url, "%s/write?db=%s&u=%s&p=%s", host, datb, user, pswd);
    } else {
        sprintf(url, "%s/write?db=%s", host, datb);
    }
    return http_post(url, body);
}



void http_serve_task(void *pvParameters) {
    struct netconn *client, *nc = netconn_new(NETCONN_TCP);
    if (!nc) {
        ESP_LOGE(TAG,"Failed to allocate socket");
        vTaskDelete(NULL);
    }
    netconn_bind(nc, IP_ADDR_ANY, 80);
    netconn_listen(nc);
    char buf[4096], rst = 0;
    while (1) {
        vTaskDelay( 0 );
        if ( netconn_accept(nc, &client) == ERR_OK ) {
            struct netbuf *nb;
            if ( netconn_recv(client, &nb) == ERR_OK ) {
                void *data;
                u16_t len;
                netbuf_data(nb, &data, &len);
                /* If request is GET, send HTML response */
                if (!strncmp(data, "GET ", 4)) {
                    ESP_LOGI(TAG,"Received data:\n%.*s\n", len, (char*)data);
                    if (!strncmp(data+4, "/favicon.ico", 12)) {
                        netconn_write(client,"HTTP/1.1 200 OK\r\n Content-Type:image/png\r\n\r\n",44,NETCONN_COPY);
                        netconn_write(client,main_www_favicon_ico,main_www_favicon_ico_len,NETCONN_NOFLAG);
                    } else {
                        u16_t vlen = (void*) strstr(data," HTTP/") - data - 10;
                        nvs_open("storage", NVS_READWRITE, &storage);
                        if (!strncmp(data+4, "/host=", 6)) {
                            strncpy(host,data+10,vlen);
                            host[vlen] = 0;
                            nvs_set_str(storage, "iflx_host", host);
                            ESP_LOGI(TAG,"Assign Influx Host: %s, len: %d",host,vlen);
                        } else if (!strncmp(data+4, "/datb=", 6)) {
                            strncpy(datb,data+10,vlen);
                            datb[vlen] = 0;
                            nvs_set_str(storage, "iflx_datb", datb);
                            ESP_LOGI(TAG,"Assign Influx Database: %s, len: %d",datb,vlen);
                        } else if (!strncmp(data+4, "/user=", 6)) {
                            strncpy(user,data+10,vlen);
                            user[vlen] = 0;
                            nvs_set_str(storage, "iflx_user", user);
                            ESP_LOGI(TAG,"Assign Influx Credentials: %s, len: %d",user,vlen);
                        } else if (!strncmp(data+4, "/pswd=", 6)) {
                            strncpy(pswd,data+10,vlen);
                            pswd[vlen] = 0;
                            nvs_set_str(storage, "iflx_pswd", pswd);
                        } else if (!strncmp(data+4, "/disconnect", 11)) {
                            ESP_LOGW(TAG,"Disconnect");
                            nvs_erase_key(storage, "ssid");
                            nvs_erase_key(storage, "pswd");
                            sta_config.sta.ssid[0] = 0;
                            sta_config.sta.password[0] = 0;
                            rst = 1;
                        } else if (!strncmp(data+4, "/reset", 6)) {
                            ESP_LOGW(TAG,"Reset");
                            rst = 1;
                        }
                        nvs_close(storage);
                        sprintf(buf, (const char*) main_www_index_html, ssid, datb, host, xTaskGetTickCount()*portTICK_PERIOD_MS/1000);
                        netconn_write(client, "HTTP/1.1 200 OK\r\n Content-Type:text/html\r\n\r\n", 44, NETCONN_COPY);
                        netconn_write(client, buf, strlen(buf), NETCONN_COPY);
                    }
                }
            }
            netbuf_delete(nb);
        }
        netconn_close(client);
        netconn_delete(client);
        if (rst) {
            esp_wifi_disconnect();
            esp_restart();
        }
    }
}

char* get_ssid() {
    return ssid;
}

void set_ssid(char *ssid_value, char *pswd_value) {
    if (ssid_value[0]) { 
        strcpy((char*)sta_config.sta.ssid, ssid_value);
        strcpy((char*)sta_config.sta.password, pswd_value);
    }
    ESP_LOGI(TAG, "Attempting to connect to '%s'", (char*)sta_config.sta.ssid);
    esp_wifi_set_config(WIFI_IF_STA, &sta_config);
    esp_wifi_connect();
}

void initialize_server() {
    xTaskCreate(&http_serve_task, "http_serve_task", 1024*8, NULL, 8, NULL);
}
