/* wifi.c | Wi-Fi Component Implementation (Including mDNS & NTP)

   This  code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/


#include <string.h>
#include "esp_event_loop.h"
#include "freertos/event_groups.h"
#include "esp_http_client.h"
#include "lwip/apps/sntp.h"
#include "lwip/api.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "lwip/apps/mdns.h"
#include "wifi.h"

#define TAG "COMPONENT:WIFI"

/* Config Web Page - Should eventually move to own file */
#define HTML "HTTP/1.1 200 OK\r\n Content-Type:text/html\r\n\r\n" \
    "<html>" \
        "<head>" \
            "<title>ESP32 Config</title>" \
            "<meta name='viewport' content='width=device-width,initial-scale=1.0'>" \
            "<style>" \
                "* {color:#fff; font-family:monospace; text-align:center; padding:0; border:none;}" \
                "input {background:#0003; width:90vw; height:8vh; font-size:3.75vh; margin:1vh 0; outline:none; -webkit-appearance: none;}" \
                "input:read-only {background:#0000}" \
            "</style>" \
        "</head>" \
        "<body style='background:#e43; color:#fff; font-family:monospace; text-align:center'>" \
            "<h2>ESP32 Config</h2><br/>" \
            "<p>NETWORK<br/><input id='ssid' type='text' value='%s' readonly /></p>" \
            "<p>UPTIME<br/><input id='time' type='text' value='%ds' readonly /></p>" \
            "<p>INFLUX HOST<br/><input id='host' type='text' value='%s' /></p>" \
            "<p>INFLUX DATABASE<br/><input id='datb' type='text' value='%s' /></p>" \
            "<p>INFLUX USER (optional)<br/><input id='user' type='text' value='' /></p>" \
            "<p>INFLUX PASSWORD (optional)<br/><input id='pswd' type='text' value='' /></p>" \
            "<iframe style='display:none'></iframe>" \
            "<script>" \
                "document.querySelectorAll('input').forEach( i => {" \
                    "i.onblur = _ => document.querySelector('iframe').contentWindow.location.href = '/' + i.id + '=' + i.value;" \
                    "i.addEventListener('keyup', e => e.keyCode == 13 && i.blur() );" \
                "});" \
            "</script>" \
        "</body>" \
    "</html>"

/* Event group to signal when Wi-Fi is connected */
static EventGroupHandle_t wifi_group;

static char recv_buf[64], url[256];
static char host[128] = CONFIG_HOST;
static char db[64] = CONFIG_DB;

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event) {
    switch(event->event_id) {
        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            xEventGroupSetBits(wifi_group, BIT0);
            ESP_LOGI(TAG, "Connected to network: %s", CONFIG_WIFI_SSID);
            initialize_sntp();
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            esp_wifi_connect();
            xEventGroupClearBits(wifi_group, BIT0);
            break;
        default:
            break;
    }
    return ESP_OK;
}

void initialize_wifi() {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t wifi_config = { .sta={.ssid=CONFIG_WIFI_SSID,.password=CONFIG_WIFI_PASSWORD} };
    wifi_group = xEventGroupCreate();
    tcpip_adapter_init();
    ESP_LOGI(TAG, "Connecting to Wi-Fi network: %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_event_loop_init(wifi_event_handler, NULL) );
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
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
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}

int http_post(char *url, char *body) {
    uint8_t status = 0;
    esp_http_client_config_t config = { .url = url };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_url(client, url);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(client, body, strlen(body));
    esp_err_t error = esp_http_client_perform(client);
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
    sprintf(url, "%s/write?db=%s", host, db);
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
    char buf[2048];
    while (1) {
        if ( netconn_accept(nc, &client) == ERR_OK ) {
            struct netbuf *nb;
            if ( netconn_recv(client, &nb) == ERR_OK ) {
                void *data;
                u16_t len;
                netbuf_data(nb, &data, &len);
                /* If request is GET, send HTML response */
                if (!strncmp(data, "GET ", 4)) {
                    ESP_LOGI(TAG,"Received data:\n%.*s\n", len, (char*)data);

                    u16_t vlen = (void*) strstr(data," HTTP/") - data - 10;
                    if (!strncmp(data+4, "/host=", 6)) {
                        strncpy(host,data+10,vlen);
                        host[vlen] = 0;
                        ESP_LOGI(TAG,"host: %s, len: %d",host,vlen);
                    } else if (!strncmp(data+4, "/datb=", 6)) {
                        strncpy(db,data+10,vlen);
                        db[vlen] = 0;
                        ESP_LOGI(TAG,"db: %s, len: %d",db,vlen);
                    } 
                    snprintf(buf, sizeof(buf), HTML, CONFIG_WIFI_SSID, xTaskGetTickCount()*portTICK_PERIOD_MS/1000, host, db);
                    netconn_write(client, buf, strlen(buf), NETCONN_COPY);
                }
            }
            netbuf_delete(nb);
        }
        ESP_LOGI(TAG,"Closing connection");
        netconn_close(client);
        netconn_delete(client);
    }
}

void initialize_server() {
    xTaskCreate(&http_serve_task, "http_serve_task", 1024*8, NULL, 8, NULL);
}