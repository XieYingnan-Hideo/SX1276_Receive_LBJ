//
// Created by FLN1021 on 2023/9/2.
//

#include "networks.h"

/* ------------------------------------------------ */
ESPTelnet telnet;
IPAddress ip;
uint16_t  port = 23;

const char* time_zone = "CST-8";
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";

struct tm time_info{};

bool isConnected() {
    return (WiFiClass::status() == WL_CONNECTED);
}
bool connectWiFi(const char* ssid, const char* password, int max_tries, int pause) {
    int i = 0;
    WiFiClass::mode(WIFI_STA);
    WiFi.begin(ssid, password);
    do {
        delay(pause);
        Serial.print(".");
    } while (!isConnected() && i++ < max_tries);
    if(isConnected())
        Serial.print("SUCCESS.");
    else
        Serial.print("FAILED.");
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    return isConnected();
}

void timeAvailable(struct timeval *t){
    Serial.println("Got time adjustment from NTP!");
    getLocalTime(&time_info);
    Serial.println(&time_info, "%Y-%m-%d %H:%M:%S");
}

/* ------------------------------------------------ */

// (optional) callback functions for telnet events
void onTelnetConnect(String ip) {
    Serial.print("[Telnet] ");
    Serial.print(ip);
    Serial.println(" connected");

    telnet.println("===[ESP32 DEV MODULE TELNET SERVICE]===");
    getLocalTime(&time_info);
    char timeStr[20];
    sprintf(timeStr, "%d-%02d-%02d %02d:%02d:%02d", time_info.tm_year+1900, time_info.tm_mon+1, time_info.tm_mday,
            time_info.tm_hour, time_info.tm_min, time_info.tm_sec);
    telnet.print("System time is ");
    telnet.print(timeStr);
    telnet.println("\nWelcome " + telnet.getIP());
    telnet.println("(Use ^] + q  to disconnect.)");
    telnet.println("=======================================");
    telnet.print("< ");
}

void onTelnetDisconnect(String ip) {
    Serial.print("[Telnet] ");
    Serial.print(ip);
    Serial.println(" disconnected");
}

void onTelnetReconnect(String ip) {
    Serial.print("[Telnet] ");
    Serial.print(ip);
    Serial.println(" reconnected");
}

void onTelnetConnectionAttempt(String ip) {
    Serial.print("[Telnet] ");
    Serial.print(ip);
    Serial.println(" tried to connected");
}

void onTelnetInput(String str) {
    // checks for a certain command
    if (str == "ping") {
        telnet.println("> pong");
        Serial.println("[Telnet]> pong");
        // disconnect the client
    } else if (str == "bye") {
        telnet.println("> disconnecting you...");
        telnet.disconnectClient();
    }
    telnet.print("< ");
}


/* ------------------------------------------------- */

void setupTelnet() {
    // passing on functions for various telnet events
    telnet.onConnect(onTelnetConnect);
    telnet.onConnectionAttempt(onTelnetConnectionAttempt);
    telnet.onReconnect(onTelnetReconnect);
    telnet.onDisconnect(onTelnetDisconnect);
    telnet.onInputReceived(onTelnetInput);

    Serial.print("[Telnet] ");
    if (telnet.begin(port)) {
        Serial.println("running");
    } else {
        Serial.println("error.");
    }
}

/* ------------------------------------------------- */

//bool ipChanged(uint16_t interval, uint64_t *timer, uint32_t *last_ip) {
//    uint32_t ip1 = WiFi.localIP();
//    if (millis()-timer >= interval){
//        if (WiFi.localIP() != ip1){
//            return true;
//        }
//    }
//    return false;
//}

/* ------------------------------------------------- */

int16_t readDataLBJ(struct PagerClient::pocsag_data *p, struct lbj_data *l){
    for (size_t i=0;i<POCDAT_SIZE;i++){
        if (p[i].is_empty)
            continue;
        switch (p[i].addr) {
            case LBJ_INFO_ADDR:
            {
                if (!p[i].func && ( p[i].str[0] == '-' || p[i].str[0] == '*' ) &&
                    (p[i].str[1]&&p[i].str[2]&&p[i].str[3]&&p[i].str[4] != '-')){

                    p[i].addr = LBJ_SYNC_ADDR; // don't know if addr = 1234008 can perform this.
                    Serial.println("Transformed type.");

                } else {
                    l->type = 0;
                    l->direction = (int8_t)p[i].func;

                    /*
                     * The standard LBJ Proximity Alarm info is formed in:
                     * ----- --- -----
                     * TRAIN SPD POS
                     * 0-4   6-8 10-14 in total of 15 nibbles / 60 bits.
                     * 'X' indicates BCH correction failed code words.
                     */
                    if (p[i].str.length() >= 5 && p[i].str[0] != 'X') {
                        for (size_t c = 0; c < 5; c++) {
                            l->train[c] = p[i].str[c];
                        }
                    }
                    if (p[i].str.length() >= 10 && p[i].str[6] != 'X') {
                        for (size_t c = 6, v = 0; c < 9; c++, v++) {
                            l->speed[v] = p[i].str[c];
                        }
                        l->speed[3] = 0;
                    }
                    if (p[i].str.length() >= 15 && p[i].str[10] != 'X') {
                        size_t v = 0;
                        for (size_t c = 10; c < 15; c++, v++) {
                            l->position[v] = p[i].str[c];
                            if(c == 13 && p[i].str[c]!=' '){
                                l->position[++v]='.';
                            } else if (c == 13) {
                                // 0.x km is better than .x km, isn't it?
                                l->position[v] = '0';
                                l->position[++v]='.';
                            }
                        }
                        l->position[v] = 0;
                    }
                    /* there are times we got this kind of message:
                     * XXXXX --- -----XXXXX20201100600430U-(2 9U- (-(2020----------XXXXX--000
                     * we have to process them as type 1.
                     */
                    if (!(p[i].str.length() == 65 || p[i].str.length() == 70)){
                        break;
                    }
                }
            }
            case LBJ_INFO2_ADDR:
            {
                l->type = 1; // type = 1 when implemented.
                if (p[i].addr == LBJ_INFO_ADDR)
                {
                    if (p[i].str.length() == 65)
                        p[i].str = p[i].str.substring(15);
                    else if (p[i].str.length() == 70)
                        p[i].str = p[i].str.substring(20);
                }
                /*
                 * The LBJ Additional info message does not appear on any standards,
                 * decoding based purely on guess and formerly received type 1 messages.
                 * A typical type 1 message is:
                 * 204U2390093130U-(2 9U- (-(202011720927939053465000
                 * |204U2|39009|3130U|-(2 9|U- (-|(2020|11720|92793|90534|65000|
                 *   0-4   5-9  10-14 15-19 20-24 25-29 30-34 35-39 40-44 45-49
                 * in which we phrase it to:
                 * |204U|23900931|30|U-(2 9U- (-(2020|117209279|39053465|000
                 *    I     II   III       IV            V         VI    VII
                 * I.   00-03   Two ASCII bytes for class, in this case 4U = 4B = K.
                 * II.  04-11   8 nibbles/32 bits for locomotive register number.
                 * III. 12-13   Unknown, usually 30, maybe some sort of sync word.
                 * IV.  14-29   4 GBK characters/8 bytes/16 nibbles/32 bits for route.
                 * V.   30-38   9 nibbles for longitude in format XXX°XX.XXXX′ E.
                 * VI.  39-46   8 nibbles for latitude in format XX°XX.XXXX′ N.
                 * VII. 47-49   Unknown, usually 000, sometimes FFF("(((")/CCC("   ")?, maybe some sort of idle word.
                 * in total of 50 nibbles / 200 bits.
                 */

                // locomotive registry number.
                if (p[i].str.length() >= 12 && p[i].str[4] != 'X' && p[i].str[5] != 'X' && p[i].str[10] != 'X') {
                    for (size_t c = 4, v = 0; c < 12; c++, v++) {
                        l->loco[v] = p[i].str[c];
                    }
                }

                // positions lon
                if (p[i].str.length() >= 39 && p[i].str[30] != 'X' && p[i].str[35] != 'X'){
                    for (size_t c = 30,v = 0;c<39;c++,v++){
                        l->pos_lon[v] = p[i].str[c];
                    }
                    for (size_t c = 30,v = 0;c<33;c++,v++){
                        l->pos_lon_deg[v] = p[i].str[c];
                    }
                    size_t v = 0;
                    for (size_t c = 33;c<39;c++,v++){
                        l->pos_lon_min[v] = p[i].str[c];
                        if (c == 34)
                            l->pos_lon_min[++v] = '.';
                    }
                }
                // position lat
                if (p[i].str.length() >= 47 && p[i].str[39] != 'X' && p[i].str[40] != 'X' && p[i].str[45] != 'X'){
                    for (size_t c = 39,v = 0;c<47;c++,v++){
                        l->pos_lat[v] = p[i].str[c];
                    }
                    for (size_t c = 39,v = 0;c<41;c++,v++){
                        l->pos_lat_deg[v] = p[i].str[c];
                    }
                    size_t v = 0;
                    for (size_t c = 41;c<47;c++,v++){
                        l->pos_lat_min[v] = p[i].str[c];
                        if (c == 42)
                            l->pos_lat_min[++v] = '.';
                    }
                }

                // reformat to hexadecimal string.
                for (char & c : p[i].str){
                    recodeBCD(&c);
                }
                // BCD to HEX and to ASCII for class
                if (p[i].str.length() >= 4 && p[i].str[0] != 'X') {
                    // this is very likely the most ugly code I've ever write, I apologize for that.
                    size_t c = 0;
                    for (size_t v = 0; v < 3; v++, c++) {
                        int a = isdigit(p[i].str[v]) ? (p[i].str[v] - '0') : (p[i].str[v] - '0' - 7),
                                b = isdigit(p[i].str[v + 1]) ? (p[i].str[++v] - '0') : (p[i].str[++v] - '0' - 7);
                        l->lbj_class[c] = (int8_t) ((a << 4) | b);
                    }
                }
                // to GBK for route.
                if (p[i].str.length() >= 20 && p[i].str[14] != 'X' && p[i].str[15] != 'X') { // Character 1
                    size_t c = 0;
                    for (size_t v = 14; v < 17; v++, c++) {
                        int a = isdigit(p[i].str[v]) ? (p[i].str[v] - '0') : (p[i].str[v] - '0' - 7),
                                b = isdigit(p[i].str[v + 1]) ? (p[i].str[++v] - '0') : (p[i].str[++v] - '0' - 7);
                        l->route[c] = (int8_t) ((a << 4) | b);
                    }
                }
                if (p[i].str.length() >= 25 && p[i].str[18] != 'X' && p[i].str[20] != 'X') {// Character 2
                    size_t c = 2;
                    for (size_t v = 18; v < 21; v++, c++) {
                        int a = isdigit(p[i].str[v]) ? (p[i].str[v] - '0') : (p[i].str[v] - '0' - 7),
                                b = isdigit(p[i].str[v + 1]) ? (p[i].str[++v] - '0') : (p[i].str[++v] - '0' - 7);
                        l->route[c] = (int8_t) ((a << 4) | b);
                    }
                }
                if (p[i].str.length() >= 30 && p[i].str[22] != 'X' && p[i].str[25] != 'X') {// Character 3,4
                    size_t c = 4;
                    for (size_t v = 22; v < 29; v++, c++) {
                        int a = isdigit(p[i].str[v]) ? (p[i].str[v] - '0') : (p[i].str[v] - '0' - 7),
                                b = isdigit(p[i].str[v + 1]) ? (p[i].str[++v] - '0') : (p[i].str[++v] - '0' - 7);
                        l->route[c] = (int8_t) ((a << 4) | b);
                    }
                }
                break;
            }
            case LBJ_SYNC_ADDR:
            {
                l->type = 2;
                if (p[i].str.length() == 5 && p[i].str[0] != 'X') {
                    for (size_t c = 1, v = 0; c < 5; c++, v++){
                        l->time[v] = p[i].str[c];
                        if(c == 2)
                            l->time[++v]=':';
                    }
                }
                break;
            }

        }
    }

    return 0;
}

void recodeBCD( char* c){
    switch (*c) {
        case '*':
        {
            *c = 'A';
            break;
        }
        case 'U':
        {
            *c = 'B';
            break;
        }
        case ' ':
        {
            *c = 'C';
            break;
        }
        case '-':
        {
            *c = 'D';
            break;
        }
        case ')':
        {
            *c = 'E';
            break;
        }
        case '(':
        {
            *c = 'F';
            break;
        }
            // 最好能改成单次转换...这样未免有点太浪费性能了
    }
}






