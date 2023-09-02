//
// Created by FLN1021 on 2023/9/2.
//

#ifndef PAGER_RECEIVE_NETWORKS_H
#define PAGER_RECEIVE_NETWORKS_H

#include <WiFi.h>
#include <ctime>
#include "sntp.h"
#include "ESPTelnet.h"
#include <RadioLib.h>
#include "unicon.h"

/* ------------------------------------------------ */
#define LBJ_INFO_ADDR 1234000
#define LBJ_INFO2_ADDR 1234002
#define LBJ_SYNC_ADDR 1234008

#define FUNCTION_DOWN 1
#define FUNCTION_UP 3

struct lbj_data{
    int8_t type = -1;
    char train[6] = "<NUL>";
    int8_t direction = -1;
    char speed[6] = "<NUL>";
    char position[6] = "<NUL>";
    char time[6] = "<NUL>";
    char info2_hex[51] = "<NUL>";
    char lbj_class[3] = "NA"; // '0X' or ' X'
    char loco[9] = "<NUL>"; // such as 23500331
    uint8_t route[17] = "********"; // 16 bytes GBK data.
    uint8_t route_utf8[17*2] = "********";
    char pos_lon_deg[4] = ""; // ---
    char pos_lon_min[8] = ""; // --.----
    char pos_lat_deg[3] = ""; // --
    char pos_lat_min[8] = "";
    char pos_lon[10] = "<NUL>"; // ---°--.----'
    char pos_lat[9] = "<NUL>"; // --°--.----'
};
/* ------------------------------------------------ */

extern const char* time_zone;
extern const char* ntpServer1;
extern const char* ntpServer2;

extern struct tm time_info;

#define WIFI_SSID       "GL-MT1300-b99"
#define WIFI_PASSWORD   "goodlife"

#define POCDAT_SIZE 16 // defines number of the pocsag data structures.

extern ESPTelnet telnet;
extern IPAddress ip;
extern uint16_t  port;

bool isConnected();
bool connectWiFi(const char* ssid, const char* password, int max_tries = 20, int pause = 500);
void timeAvailable(struct timeval *t);

void onTelnetConnect(String ip);
void onTelnetDisconnect(String ip);
void onTelnetReconnect(String ip);
void onTelnetConnectionAttempt(String ip);
void onTelnetInput(String str);

void setupTelnet();

//extern bool ipChanged(uint16_t interval);

int16_t readDataLBJ(struct PagerClient::pocsag_data *p, struct lbj_data *l);
void recodeBCD(char *c);

int enc_unicode_to_utf8_one(unsigned long unic,unsigned char *pOutput);
void gbk2utf8(uint8_t *gbk,uint8_t *utf8,size_t gbk_len);

#endif //PAGER_RECEIVE_NETWORKS_H
