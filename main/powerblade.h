/* powerblade.h | PowerBlade Processing Component Header

   This code is in the Public Domain (or CC0 licensed, at your option).

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/


#ifndef _POWERBLADE_H_
#define _POWERBLADE_H_

#include <stdint.h>
#include <time.h>

/* Struct for parsed PowerBlade data */
typedef struct {
    uint8_t id[6];  // device id
    uint32_t sn;    // sequence number
    double vr;      // rms voltage
    double rp;      // real power in watts
    double ap;      // apparent power in volt-amperes (~watts)
    double wh;      // energy in watt-hours
    double pf;      // power factor
    time_t ts;      // time in seconds
    long tn;    // remainder in nanoseconds
} powerblade_item;

void powerblade_item_to_influx_string(powerblade_item *item, char *out);

int8_t parse_powerblade_data(uint8_t *device_id, uint8_t *data, uint8_t length, powerblade_item *item);

#define is_powerblade_device(id,ad_len) (id[0]==0xC0 && id[1]==0x98 && id[2]==0xE5 && id[3]==0x70 && ad_len>18)

#define is_powerblade_data_packet(ad) (ad[3]>=0x17 && ad[7]==0x11 && ad[6] == 0x02 && ad[5] == 0xE0 && ad[8]==0x02)

#endif
