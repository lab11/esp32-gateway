/* powerblade.c | PowerBlade Processing Component Implementation

   This  code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/


#include <math.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "powerblade.h"

#define TAG "COMPONENT:POWERBLADE"

/* Template Influx string for PowerBlade data */
#define PB_INFLUX "data," \
                  "device_class=\"PowerBlade\"," \
                  "device_id=\"%s\"," \
                  "gateway_id=\"%s\"," \
                  "receiver=\"esp32-gateway\" " \
                  "sequence_number=%lu," \
                  "rms_voltage=%.2f," \
                  "power=%.2f," \
                  "apparent_power=%.2f," \
                  "energy=%.2f," \
                  "power_factor=%.2f " \
                  "%lld%09ld\n" // timestamp

/* Log template :     time | id |  seq # |  rms voltage |    power |    apparent power |    energy | p factor */
#define PB_LOG "%lld%09ld ns | %s | #%010lu | %10.2f V_rms | %10.2f W | %10.2f W_apparent | %10.2f Wh | %1.2f pf\n"

#define get_id_string(id, id_string) for(int i=0; i<6; i++) sprintf(id_string+3*i, "%02x%s", id[i], i<5 ? ":" : "")

static char gateway[17], id[17];
struct timespec tv = {0, 0};

void get_mac() {
    uint8_t mac[6];
    ESP_ERROR_CHECK( esp_efuse_mac_get_default(mac) );
    get_id_string(mac, gateway);
}

void log_powerblade_item(powerblade_item *item) {
    char id[17];
    get_id_string(item->id, id);
    printf(PB_LOG, item->ts, item->tn, id, item->sn, item->vr, item->rp, item->ap, item->wh, item->pf);
}

int8_t parse_powerblade_data(uint8_t *id, uint8_t *data, uint8_t length, powerblade_item *item) {
    /* PowerBlade Device Check */
    if (!is_powerblade_device(id,length)) {
        return 0;
    }

    /* PowerBlade Data Packet Check */
    if (!(is_powerblade_data_packet(data))) {
        ESP_LOGW(TAG,"No parseable data in this packet");
        return 0;
    }

    /* Retrieve system time */
    clock_gettime(CLOCK_REALTIME, &tv);

    /* Parse packet */
    /* Example AD: 02 01 06 17 ff e0 02 11 02 00 54 8a 1d 4f ff 79 09 c8 0c 83 0c b7 01 11 98 ba 42 */
    uint32_t sequence_num   = data[9]<<24 | data[10]<<16 | data[11]<<8 | data[12];
    uint32_t pscale         = data[13]<<8 | data[14];
    uint32_t vscale         = data[15];
    uint32_t whscale        = data[16];
    uint32_t v_rms          = data[17];
    uint32_t real_power     = data[18]<<8 | data[19];
    uint32_t apparent_power = data[20]<<8 | data[21];
    uint32_t watt_hours     = data[22]<<24 | data[23]<<16 | data[24]<<8 | data[25];
    double volt_scale       = vscale / 200.0;
    double power_scale      = (pscale & 0x0FFF) / pow(10.0, (pscale & 0xF000)>>12);
    item->sn                 = sequence_num;
    item->ts                 = tv.tv_sec;
    item->tn                 = tv.tv_nsec;
    item->vr                 = v_rms * volt_scale;
    item->rp                 = real_power * power_scale;
    item->ap                 = apparent_power * power_scale;
    item->wh                 = volt_scale > 0 ? (watt_hours << whscale) * (power_scale / 3600.0) : watt_hours;
    item->pf                 = item->rp / item->ap;
    memcpy(item->id, id, 6);

    /* Output to serial */
    log_powerblade_item(item);

    /* Time check - if not set, do not store/send data */
    if (tv.tv_sec < 1500000000) {
        ESP_LOGW(TAG,"Time not set. Check Internet connection...");
        return 0;
    }
    
    return 1;
}

void powerblade_item_to_influx_string(powerblade_item *item, char *out) {
    if (!gateway[0]) {
        get_mac();
    }
    get_id_string(item->id, id);
    sprintf(out, PB_INFLUX, id, gateway, item->sn, item->vr, item->rp, item->ap, item->wh, item->pf, item->ts, item->tn);
}
