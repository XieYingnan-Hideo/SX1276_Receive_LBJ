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

void silentConnect(const char* ssid, const char* password){
    WiFiClass::mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    WiFi.begin(ssid, password);
}

void changeCpuFreq(uint32_t freq_mhz){ //todo 这个如果一直没Wifi的话，变频会导致又开始扫描WIFI，找到这个扫描超时开关，加进来。
    if (isConnected() || no_wifi){
        if (ets_get_cpu_frequency() != freq_mhz)
            setCpuFrequencyMhz(freq_mhz);
    }
    else {
        WiFiClass::mode(WIFI_OFF);
        if (ets_get_cpu_frequency() != freq_mhz)
            setCpuFrequencyMhz(freq_mhz);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        WiFiClass::mode(WIFI_STA);
//        WiFi.setAutoReconnect(true);
//        WiFi.persistent(true);
    }
}

/* ------------------------------------------------ */

void timeAvailable(struct timeval *t){
    Serial.println("[SNTP] Got time adjustment from NTP!");
    getLocalTime(&time_info);
    Serial.println(&time_info, "[SNTP] %Y-%m-%d %H:%M:%S");
}

//void printLocalTime()
//{
//    struct tm timeinfo;
//    if(!getLocalTime(&timeinfo)){
//        Serial.println("No time available (yet)");
//        return;
//    }
//    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
//}

/* ------------------------------------------------ */

// (optional) callback functions for telnet events
void onTelnetConnect(String ip) {
    Serial.print("[Telnet] ");
    Serial.print(ip);
    Serial.println(" connected");

    telnet.println("===[ESP32 DEV MODULE TELNET SERVICE]===");
    getLocalTime(&time_info,10);
    char timeStr[20];
    sprintf(timeStr, "%d-%02d-%02d %02d:%02d:%02d", time_info.tm_year+1900, time_info.tm_mon+1, time_info.tm_mday,
            time_info.tm_hour, time_info.tm_min, time_info.tm_sec);
    telnet.print("System time is ");
    telnet.print(timeStr);
    telnet.println("\n\rWelcome " + telnet.getIP());
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
    bool ext_call = false;
    size_t args_count = 3;
    char strc[256];
    str.toCharArray(strc,256);
    String args[3];
    bool toolong = false;
    for (size_t i=0,c=0;i<256;i++)
    {
        if (strc[i] == 0)
            break;
        if (strc[i] == ' ') {
            i++;c++;
        }
        if (c>args_count-1) {
            toolong = true;
            break;
        }
        args[c] += strc[i];
    }
    if (!toolong) {
        if (args[0] == "test") {
            if (args[1] == "test") {
                telnet.println("> Args test.");
            } else
                telnet.println("> Unknown Command.");
        }
        if (args[0] == "log") {
            if (args[1] == "read") {
                bool valid = true;
                if (args[2].length() == 0)
                    valid = false;
                for(auto c:args[2]){
                    if(!isDigit(c))
                        valid = false;
                }
                if (!valid)
                    telnet.println("> Invalid Format, digits only.");
                else{
                    if(SD_LOG::status())
                        SD_LOG::printTel(std::stoi(args[2].c_str()),telnet);
//                        telPrintLog(std::stoi(args[2].c_str()));
                    else
                        telnet.println("> Can not access SD card.");
                }
            } else if (args[1] == "status"){
                if (SD_LOG::status())
                    telnet.println("> True.");
                else
                    telnet.println("> False.");
            }
            else
                telnet.println("> Unknown Command.");
        }
    }

    // checks for a certain command
    if (str == "ping") {
        telnet.println("> pong");
        Serial.println("[Telnet] > pong");
        // disconnect the client
    } else if (str == "bye") {
        telnet.println("> disconnecting you...");
        telnet.disconnectClient();
    } else if (str == "read") {
        if(SD_LOG::status())
            telPrintLog(1000);
        else
            telnet.println("> Can not access SD card.");
    } else if (str == "bat")
        telnet.printf("Current Battery Voltage %1.2fV\n",battery.readVoltage()*2);
    else if (str == "rssi"){
        give_tel_rssi = true;
        ext_call = true;
    }
    else if (str == "gain") {
        give_tel_gain = true;
        ext_call = true;
    }
    if (!ext_call)
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
                    goto lbj_sync;
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
                    if (!(p[i].str.length() == 65 || p[i].str.length() >= 70 )){
                        break;
                    }
                }
            }
            case LBJ_INFO2_ADDR:
            {
                l->type = 1; // type = 1 when implemented.
                String buffer;
                if (p[i].addr == LBJ_INFO_ADDR)
                {
                    if (p[i].str.length() == 65)
                        buffer = p[i].str.substring(15);
                    else if (p[i].str.length() == 70 || (p[i].str.length() >= 70 && p[i].str[67] == '0'
                            && p[i].str[68] == '0' && p[i].str[69] == '0'))
                        buffer = p[i].str.substring(20);
                    else
                        break;
                } else
                    buffer = p[i].str;
                if (l->direction == -1)
                    l->direction = (int8_t)p[i].func;
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
                 * In total of 50 nibbles / 200 bits.
                 */

                // reformat to hexadecimal string.
                for (char &c: buffer) {
                    recodeBCD(&c, &l->info2_hex);
                }

                // locomotive registry number.
                if (buffer.length() >= 12 && buffer[4] != 'X' && buffer[5] != 'X' && buffer[10] != 'X') {
                    for (size_t c = 4, v = 0; c < 12; c++, v++) {
                        l->loco[v] = buffer[c];
                    }
                    String type;
                    for (size_t c=0;c<3;c++)
                    {
                        if(isdigit(l->loco[c]))
                            type += l->loco[c];
                    }
                    if (type.length()==3)
                        l->loco_type = locos[std::stoi(type.c_str())];
                }

                // positions lon
                if (buffer.length() >= 39 && buffer[30] != 'X' && buffer[35] != 'X'){
                    for (size_t c = 30,v = 0;c<39;c++,v++){
                        l->pos_lon[v] = buffer[c];
                    }
                    for (size_t c = 30,v = 0;c<33;c++,v++){
                        l->pos_lon_deg[v] = buffer[c];
                    }
                    size_t v = 0;
                    for (size_t c = 33;c<39;c++,v++){
                        l->pos_lon_min[v] = buffer[c];
                        if (c == 34)
                            l->pos_lon_min[++v] = '.';
                    }
                }
                // position lat
                if (buffer.length() >= 47 && buffer[39] != 'X' && buffer[40] != 'X' && buffer[45] != 'X'){
                    for (size_t c = 39,v = 0;c<47;c++,v++){
                        l->pos_lat[v] = buffer[c];
                    }
                    for (size_t c = 39,v = 0;c<41;c++,v++){
                        l->pos_lat_deg[v] = buffer[c];
                    }
                    size_t v = 0;
                    for (size_t c = 41;c<47;c++,v++){
                        l->pos_lat_min[v] = buffer[c];
                        if (c == 42)
                            l->pos_lat_min[++v] = '.';
                    }
                }

                // BCD to HEX and to ASCII for class  todo: Add range filter to filter out of table characters, limit by GBK.
                if (l->info2_hex.length() >= 4 && l->info2_hex[0] != 'X') {
                    // this is very likely the most ugly code I've ever written, I apologize for that.
                    size_t c = 0;
                    for (size_t v = 0; v < 3; v++, c++) {
                        int a = isdigit(l->info2_hex[v]) ? (l->info2_hex[v] - '0') : (l->info2_hex[v] - '0' - 7),
                                b = isdigit(l->info2_hex[v + 1]) ? (l->info2_hex[++v] - '0') : (l->info2_hex[++v] - '0' - 7);
                        auto ch = (int8_t) ((a << 4) | b);
                        if (ch > 0x1F && ch < 0x7F && ch != 0x22)
                            l->lbj_class[c] = ch;
                    }
                }
                // to GBK for route.
                if (l->info2_hex.length() >= 20 && l->info2_hex[14] != 'X' && l->info2_hex[15] != 'X') { // Character 1
                    size_t c = 0;
                    for (size_t v = 14; v < 17; v++, c++) {
                        int a = isdigit(l->info2_hex[v]) ? (l->info2_hex[v] - '0') : (l->info2_hex[v] - '0' - 7),
                                b = isdigit(l->info2_hex[v + 1]) ? (l->info2_hex[++v] - '0') : (l->info2_hex[++v] - '0' - 7);
                        auto ch = (int8_t) ((a << 4) | b);
                        if ( (uint8_t) ch >= 0xA0 || ch == 0x20 )
                            l->route[c] = ch;
                    }
                }
                if (l->info2_hex.length() >= 25 && l->info2_hex[18] != 'X' && l->info2_hex[20] != 'X') {// Character 2
                    size_t c = 2;
                    for (size_t v = 18; v < 21; v++, c++) {
                        int a = isdigit(l->info2_hex[v]) ? (l->info2_hex[v] - '0') : (l->info2_hex[v] - '0' - 7),
                                b = isdigit(l->info2_hex[v + 1]) ? (l->info2_hex[++v] - '0') : (l->info2_hex[++v] - '0' - 7);
                        auto ch = (int8_t) ((a << 4) | b);
                        if ( (uint8_t) ch >= 0xA0 || ch == 0x20 )
                            l->route[c] = ch;
                    }
                }
                if (l->info2_hex.length() >= 30 && l->info2_hex[22] != 'X' && l->info2_hex[25] != 'X') {// Character 3,4
                    size_t c = 4;
                    for (size_t v = 22; v < 29; v++, c++) {
                        int a = isdigit(l->info2_hex[v]) ? (l->info2_hex[v] - '0') : (l->info2_hex[v] - '0' - 7),
                                b = isdigit(l->info2_hex[v + 1]) ? (l->info2_hex[++v] - '0') : (l->info2_hex[++v] - '0' - 7);
                        auto ch = (int8_t) ((a << 4) | b);
                        if ( (uint8_t) ch >= 0xA0 || ch == 0x20 )
                            l->route[c] = ch;
                    }
                }
                gbk2utf8(l->route,l->route_utf8,17);
                break;
            }
            case LBJ_SYNC_ADDR:
            {
                lbj_sync:
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

void recodeBCD( const char* c, String* v){
    switch (*c) {
        case '*':
        {
            *v += 'A';
            break;
        }
        case 'U':
        {
            *v += 'B';
            break;
        }
        case ' ':
        {
            *v += 'C';
            break;
        }
        case '-':
        {
            *v += 'D';
            break;
        }
        case ')':
        {
            *v += 'E';
            break;
        }
        case '(':
        {
            *v += 'F';
            break;
        }
        default:
        {
            *v += *c;
            break;
        }
            // 最好能改成单次转换...这样未免有点太浪费性能了
    }
}

/*---------------------------------------------------------*/

int enc_unicode_to_utf8_one(unsigned long unic,unsigned char *pOutput)
{
    assert(pOutput != nullptr);

    if (unic <= 0x0000007F)
    {
        // * U-00000000 - U-0000007F:  0xxxxxxx
        *pOutput     = (unic & 0x7F);
        return 1;
    }
    else if (unic >= 0x00000080 && unic <= 0x000007FF)
    {
        // * U-00000080 - U-000007FF:  110xxxxx 10xxxxxx
        *(pOutput + 1) = (unic & 0x3F) | 0x80;
        *pOutput     = ((unic >> 6) & 0x1F) | 0xC0;
        return 2;
    }
    else if (unic >= 0x00000800 && unic <= 0x0000FFFF)
    {
        // * U-00000800 - U-0000FFFF:  1110xxxx 10xxxxxx 10xxxxxx
        *(pOutput + 2) = (unic & 0x3F) | 0x80;
        *(pOutput + 1) = ((unic >>  6) & 0x3F) | 0x80;
        *pOutput     = ((unic >> 12) & 0x0F) | 0xE0;
        return 3;
    }
    else if (unic >= 0x00010000 && unic <= 0x001FFFFF)
    {
        // * U-00010000 - U-001FFFFF:  11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        *(pOutput + 3) = (unic & 0x3F) | 0x80;
        *(pOutput + 2) = ((unic >>  6) & 0x3F) | 0x80;
        *(pOutput + 1) = ((unic >> 12) & 0x3F) | 0x80;
        *pOutput     = ((unic >> 18) & 0x07) | 0xF0;
        return 4;
    }
    else if (unic >= 0x00200000 && unic <= 0x03FFFFFF)
    {
        // * U-00200000 - U-03FFFFFF:  111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
        *(pOutput + 4) = (unic & 0x3F) | 0x80;
        *(pOutput + 3) = ((unic >>  6) & 0x3F) | 0x80;
        *(pOutput + 2) = ((unic >> 12) & 0x3F) | 0x80;
        *(pOutput + 1) = ((unic >> 18) & 0x3F) | 0x80;
        *pOutput     = ((unic >> 24) & 0x03) | 0xF8;
        return 5;
    }
    else if (unic >= 0x04000000 && unic <= 0x7FFFFFFF)
    {
        // * U-04000000 - U-7FFFFFFF:  1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
        *(pOutput + 5) = (unic & 0x3F) | 0x80;
        *(pOutput + 4) = ((unic >>  6) & 0x3F) | 0x80;
        *(pOutput + 3) = ((unic >> 12) & 0x3F) | 0x80;
        *(pOutput + 2) = ((unic >> 18) & 0x3F) | 0x80;
        *(pOutput + 1) = ((unic >> 24) & 0x3F) | 0x80;
        *pOutput     = ((unic >> 30) & 0x01) | 0xFC;
        return 6;
    }

    return 0;
}

void gbk2utf8(const char *gbk1,char *utf8s,size_t gbk_len){
    uint16_t unic[gbk_len];
    uint8_t gbk[gbk_len];

    size_t c=0;
    for (size_t i=0;i<gbk_len;i++){
        gbk[i]=(uint8_t)gbk1[i];
    }
    for (size_t i=0;i<gbk_len;i++,c++){
        if (gbk[i] < 0x80){
            unic[c] = gbk[i];
        } else {
            unic[c] = ff_oem2uni((uint16_t) (gbk[i] << 8 | gbk[i + 1]), 936);
            i++;
        }
    }
    c=0;
    size_t i=0;
    uint8_t utf8[gbk_len*2];
    for (;i<gbk_len*2;i++,c++){
        uint8_t ut8[4];
        int r = enc_unicode_to_utf8_one(unic[c],ut8);
        if (i+4<gbk_len*2) {
            if (r == 1) utf8[i] = ut8[0];
            else if (r == 2) utf8[i] = ut8[0], utf8[++i] = ut8[1];
            else if (r == 3) utf8[i] = ut8[0], utf8[++i] = ut8[1], utf8[++i] = ut8[2];
            else if (r == 4) utf8[i] = ut8[0], utf8[++i] = ut8[1], utf8[++i] = ut8[2], utf8[++i] = ut8[3];
        }
    }
    for (size_t v=0;v<gbk_len*2;v++){
        utf8s[v]=(char)utf8[v];
    }
}

/*---------------------------------------------------------*/
void telPrintf(bool time_stamp, const char* format, ...) { // Generated by ChatGPT.
    char buffer[256]; // 创建一个足够大的缓冲区来容纳格式化后的字符串
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args); // 格式化字符串
    va_end(args);

    // 输出到 Telnet
    if (telnet.online) { // code from Multimon-NG unixinput.c 还得是multimon-ng，chatgpt写了四五个版本都没解决。
        if (is_startline){
            telnet.print("\r> ");
            if (time_stamp && getLocalTime(&time_info,0))
                telnet.printf("\r%d-%02d-%02d %02d:%02d:%02d > ", time_info.tm_year+1900, time_info.tm_mon+1,
                              time_info.tm_mday,time_info.tm_hour, time_info.tm_min, time_info.tm_sec);
            is_startline = false;
        }
        telnet.print(buffer);
        if (nullptr != strchr(buffer,'\n')) {
            is_startline = true;
            telnet.print("\r< ");
        }
    }
}
void telPrintLog(int chars){
    File log = SD_LOG::logFile('r');
    uint32_t pos,left;
    if (chars < log.size())
        pos = log.size() - chars;
    else
        pos = 0;left = chars - log.size();
    String line;
    if(!log.seek(pos))
        Serial.println("[SDLOG] seek failed!");
    while (log.available()){
        line = log.readStringUntil('\n');
        if (line){
            telnet.print(line);
            telnet.print("\n");
        }
        else
            telPrintf(false,"[SDLOG] Read failed!");
    }
    if (left)

    SD_LOG::reopen();
}

void printDataSerial(PagerClient::pocsag_data *p,const struct lbj_data& l,const struct rx_info& r){
    /* [LBJ] 方向: 下行  车次: 12345  速度: 122km/h  公里标: 1255.5km [R:-85.6 dBm/F:123.54Hz][1234000/1: 52011  70  1503][E:05/10/100]
     * [LBJ] 当前时间 12:45  [R:-85.6 dBm/F:123.54Hz][1234008/1:-1245][E:00/00/32]
     * ==================================================================================
     * [LBJ] 方向: 下行     车次:   12345   速度: 103 km/h  公里标: 1255.3 km
     * [LBJ] 线路: 京西动走  车号: 23600045  位置: 39°06.0616′ 117°19.0820′
     * ----------------------------------------------------------------------------------
     * [RXI] [R:-85.6 dBm/F:123.54Hz/E:05/10/100]
     * [PGR] [1234000/1: 52011  70  1503]
     * [PGR] [1234002/1: 20202350019930U)*9UU*6 (-(20211171908203906061600][E:12/12/200]
     * ==================================================================================
     */
    switch (l.type) {
        case 0:
        {
            if (l.direction == FUNCTION_UP)
                Serial.print("[LBJ] 方向: 上行  ");
            else if (l.direction == FUNCTION_DOWN)
                Serial.print("[LBJ] 方向: 下行  ");
            else
                Serial.printf("[LBJ] 方向: %4d  ",l.direction);
            Serial.printf("车次: %s  速度: %s KM/H  公里标: %s KM  ",l.train,l.speed,l.position);
            break;
        }
        case 1:
        {
            Serial.printf("==================================================================================\n");
            if (l.direction == FUNCTION_UP)
                Serial.print("[LBJ] 方向: 上行     ");
            else if (l.direction == FUNCTION_DOWN)
                Serial.print("[LBJ] 方向: 下行     ");
            else
                Serial.printf("[LBJ] 方向: %3d     ",l.direction);
            Serial.printf("车次: %s%s   速度: %s KM/H  公里标: %s KM \n",l.lbj_class,l.train,l.speed,l.position);
            Serial.printf("[LBJ] 线路: %s 车号: %s  ",l.route_utf8,l.loco);
            if (l.pos_lat_deg[1] && l.pos_lat_min[1])
                Serial.printf("位置: %s°%2s′ ", l.pos_lat_deg, l.pos_lat_min);
            else
                Serial.printf("位置: %s ",l.pos_lat);
            if (l.pos_lon_deg[1] && l.pos_lon_min[1])
                Serial.printf("%s°%2s′ \n",l.pos_lon_deg,l.pos_lon_min);
            else
                Serial.printf("%s \n",l.pos_lon);
            Serial.printf("----------------------------------------------------------------------------------\n");
            Serial.printf("[RXI] [R:%3.1f dBm/F:%4.2f Hz]\n", r.rssi, r.fer);
            for (size_t i = 0; i < POCDAT_SIZE; i++) {
                if (p[i].is_empty)
                    continue;
                Serial.printf("[PGR] [%d/%d:%s]", p[i].addr, p[i].func, p[i].str.c_str());
                Serial.printf("[E:%02d/%02d/%zu]\n", p[i].errs_uncorrected, p[i].errs_total, (p[i].len / 5) * 32);
            }
            Serial.printf("==================================================================================\n");
            break;
        }
        case 2:
        {
            Serial.printf("[LBJ] 当前时间 %s  ",l.time);
            break;
        }
    }
    if (l.type != 1) {
        Serial.printf("[R:%3.1f dBm/F:%5.2f Hz]", r.rssi, r.fer);
        for (size_t i = 0; i < POCDAT_SIZE; i++) {
            if (p[i].is_empty)
                continue;
            Serial.printf("[%d/%d:%s]", p[i].addr, p[i].func, p[i].str.c_str());
            Serial.printf("[E:%02d/%02d/%zu]", p[i].errs_uncorrected, p[i].errs_total, (p[i].len / 5) * 32);
        }
        Serial.printf("\n");
    }
}
void appendDataLog(PagerClient::pocsag_data *p, const struct lbj_data& l, const struct rx_info& r){
    /* [LBJ] 方向: 下行  车次: 12345  速度: 122km/h  公里标: 1255.5km [R:-85.6 dBm/F:123.54Hz][1234000/1: 52011  70  1503][E:05/10/100]
     * [LBJ] 当前时间 12:45  [R:-85.6 dBm/F:123.54Hz][1234008/1:-1245][E:00/00/32]
     * ==================================================================================
     * [LBJ] 方向: 下行     车次:   12345   速度: 103 km/h  公里标: 1255.3 km
     * [LBJ] 线路: 京西动走  车号: 23600045  位置: 39°06.0616′ 117°19.0820′
     * ----------------------------------------------------------------------------------
     * [RXI] [R:-85.6 dBm/F:123.54Hz/E:05/10/100]
     * [PGR] [1234000/1: 52011  70  1503]
     * [PGR] [1234002/1: 20202350019930U)*9UU*6 (-(20211171908203906061600][E:12/12/200]
     * ==================================================================================
     */
    switch (l.type) {
        case 0:
        {
            if (l.direction == FUNCTION_UP)
                SD_LOG::appendBuffer("[LBJ] 方向: 上行  ");
            else if (l.direction == FUNCTION_DOWN)
                SD_LOG::appendBuffer("[LBJ] 方向: 下行  ");
            else
                SD_LOG::appendBuffer("[LBJ] 方向: %4d  ",l.direction);
            SD_LOG::appendBuffer("车次: %s  速度: %s KM/H  公里标: %s KM  ",l.train,l.speed,l.position);
            break;
        }
        case 1:
        {
            SD_LOG::appendBuffer("==================================================================================\n");
            if (l.direction == FUNCTION_UP)
                SD_LOG::appendBuffer("[LBJ] 方向: 上行     ");
            else if (l.direction == FUNCTION_DOWN)
                SD_LOG::appendBuffer("[LBJ] 方向: 下行     ");
            else
                SD_LOG::appendBuffer("[LBJ] 方向: %3d     ",l.direction);
            SD_LOG::appendBuffer("车次: %s%s   速度: %s KM/H  公里标: %s KM \n",l.lbj_class,l.train,l.speed,l.position);
            SD_LOG::appendBuffer("[LBJ] 线路: %s 车号: %s  ",l.route_utf8,l.loco);
            if (l.pos_lat_deg[1] && l.pos_lat_min[1])
                SD_LOG::appendBuffer("位置: %s°%2s′ ", l.pos_lat_deg, l.pos_lat_min);
            else
                SD_LOG::appendBuffer("位置: %s ",l.pos_lat);
            if (l.pos_lon_deg[1] && l.pos_lon_min[1])
                SD_LOG::appendBuffer("%s°%2s′ \n",l.pos_lon_deg,l.pos_lon_min);
            else
                SD_LOG::appendBuffer("%s \n",l.pos_lon);
            SD_LOG::appendBuffer("----------------------------------------------------------------------------------\n");
            SD_LOG::appendBuffer("[RXI] [R:%3.1f dBm/F:%4.2f Hz]\n", r.rssi, r.fer);
            for (size_t i = 0; i < POCDAT_SIZE; i++) {
                if (p[i].is_empty)
                    continue;
                SD_LOG::appendBuffer("[PGR] [%d/%d:%s]", p[i].addr, p[i].func, p[i].str.c_str());
                SD_LOG::appendBuffer("[E:%02d/%02d/%zu]\n", p[i].errs_uncorrected, p[i].errs_total, (p[i].len / 5) * 32);
            }
            SD_LOG::appendBuffer("==================================================================================\n");
            break;
        }
        case 2:
        {
            SD_LOG::appendBuffer("[LBJ] 当前时间 %s  ",l.time);
            break;
        }
    }
    if (l.type != 1) {
        SD_LOG::appendBuffer("[R:%3.1f dBm/F:%5.2f Hz]", r.rssi, r.fer);
        for (size_t i = 0; i < POCDAT_SIZE; i++) {
            if (p[i].is_empty)
                continue;
            SD_LOG::appendBuffer("[%d/%d:%s]", p[i].addr, p[i].func, p[i].str.c_str());
            SD_LOG::appendBuffer("[E:%02d/%02d/%zu]", p[i].errs_uncorrected, p[i].errs_total, (p[i].len / 5) * 32);
        }
        SD_LOG::appendBuffer("\n");
    }
    SD_LOG::sendBufferLOG();
}
void printDataTelnet(PagerClient::pocsag_data *p,const struct lbj_data& l,const struct rx_info& r){
    /* [LBJ] 方向: 下行  车次: 12345  速度: 122km/h  公里标: 1255.5km [R:-85.6 dBm/F:123.54Hz]
     * [LBJ] 当前时间 12:45  [R:-85.6 dBm/F:123.54Hz]
     * ==================================================================================
     * [LBJ] 方向: 下行     车次:   12345   速度: 103 km/h  公里标: 1255.3 km
     * [LBJ] 线路: 京西动走  车号: 23600045  位置: 39°06.0616′ 117°19.0820′
     * ----------------------------------------------------------------------------------
     * [RXI] [R:-85.6 dBm/F:123.54Hz/E:05/10/100]
     * [PGR] [1234000/1: 52011  70  1503]
     * [PGR] [1234002/1: 20202350019930U)*9UU*6 (-(20211171908203906061600][E:12/12/200]
     * ==================================================================================
     */
    switch (l.type) {
        case 0:
        {
            if (l.direction == FUNCTION_UP)
                telPrintf(true,"[LBJ] 方向: 上行  ");
            else if (l.direction == FUNCTION_DOWN)
                telPrintf(true,"[LBJ] 方向: 下行  ");
            else
                telPrintf(true,"[LBJ] 方向: %3d  ",l.direction);
            telPrintf(true,"车次: %s  速度: %s KM/H  公里标: %s KM  ",l.train,l.speed,l.position);
            break;
        }
        case 1:
        {
            telPrintf(true,"==================================================================================\n");
            if (l.direction == FUNCTION_UP)
                telPrintf(true,"[LBJ] 方向: 上行     ");
            else if (l.direction == FUNCTION_DOWN)
                telPrintf(true,"[LBJ] 方向: 下行     ");
            else
                telPrintf(true,"[LBJ] 方向: %4d     ",l.direction);
            telPrintf(true,"车次: %s%s   速度: %s KM/H  公里标: %s KM \n",l.lbj_class,l.train,l.speed,l.position);
            telPrintf(true,"[LBJ] 线路: %s 车号: %s  ",l.route_utf8,l.loco);
            if (l.pos_lat_deg[1] && l.pos_lat_min[1])
                telPrintf(true,"位置: %s°%2s′ ", l.pos_lat_deg, l.pos_lat_min);
            else
                telPrintf(true,"位置: %s ",l.pos_lat);
            if (l.pos_lon_deg[1] && l.pos_lon_min[1])
                telPrintf(true,"%s°%2s′ \n",l.pos_lon_deg,l.pos_lon_min);
            else
                telPrintf(true,"%s \n",l.pos_lon);
            telPrintf(true,"----------------------------------------------------------------------------------\n");
            telPrintf(true,"[RXI] [R:%3.1f dBm/F:%4.2f Hz]\n", r.rssi, r.fer);
            for (size_t i = 0; i < POCDAT_SIZE; i++) {
                if (p[i].is_empty)
                    continue;
                telPrintf(true,"[PGR] [%d/%d:%s]", p[i].addr, p[i].func, p[i].str.c_str());
                telPrintf(true,"[E:%02d/%02d/%zu]\n", p[i].errs_uncorrected, p[i].errs_total, (p[i].len / 5) * 32);
            }
            telPrintf(true,"==================================================================================\n");
            break;
        }
        case 2:
        {
            telPrintf(true,"[LBJ] 当前时间 %s  ",l.time);
            break;
        }
    }
    if (l.type != 1) {
        telPrintf(true,"[R:%3.1f dBm/F:%5.2f Hz]\n", r.rssi, r.fer);
    }
}

// TODO: Every append sentence costs 20 ms, now two functions costs 200+ in total, try reduce time consumption.
// Ether by reducing amount of append calling or simplify append function.
// 可以考虑建立一个BUFFER，先都写进这个buffer然后一次写入sd卡

void appendDataCSV(PagerClient::pocsag_data *p, const struct lbj_data &l, const struct rx_info &r) {
    // 电压,系统时间,日期,时间,LBJ时间,方向,级别,车次,速度,公里标,机车编号,线路,纬度,经度,HEX,RSSI,FER,原始数据,错误,错误率
    // LBJ时间,方向,级别,车次,速度,公里标,机车编号,线路,纬度,经度,HEX,RSSI,FER,原始数据,错误,错误率
    switch (l.type) {
        case 0:
        {
            if (l.direction == FUNCTION_UP)
                SD_LOG::appendBufferCSV(",上行,");
            else if (l.direction == FUNCTION_DOWN)
                SD_LOG::appendBufferCSV(",下行,");
            else
                SD_LOG::appendBufferCSV(",%d,",l.direction);
            SD_LOG::appendBufferCSV(",%s,%s,%s,,,,,,",l.train,l.speed,l.position);
            break;
        }
        case 1:
        {
            if (l.direction == FUNCTION_UP)
                SD_LOG::appendBufferCSV(",上行,");
            else if (l.direction == FUNCTION_DOWN)
                SD_LOG::appendBufferCSV(",下行,");
            else
                SD_LOG::appendBufferCSV(",%d,",l.direction);
            SD_LOG::appendBufferCSV("%s,%s,%s,%s,%s,%s,",l.lbj_class,l.train,l.speed,l.position,l.loco,l.route_utf8);
            if (l.pos_lat_deg[1] && l.pos_lat_min[1])
                SD_LOG::appendBufferCSV("%s°%2s′,", l.pos_lat_deg, l.pos_lat_min);
            else
                SD_LOG::appendBufferCSV("%s,",l.pos_lat);
            if (l.pos_lon_deg[1] && l.pos_lon_min[1])
                SD_LOG::appendBufferCSV("%s°%2s′,",l.pos_lon_deg,l.pos_lon_min);
            else
                SD_LOG::appendBufferCSV("%s,",l.pos_lon);
            SD_LOG::appendBufferCSV("\"%s\",%3.1f,%4.2f,",l.info2_hex.c_str(),r.rssi, r.fer);
            SD_LOG::appendBufferCSV("\"");
            uint8_t err_ttl=0,err_un=0,len=0;
            for (size_t i = 0; i < POCDAT_SIZE; i++) {
                if (p[i].is_empty)
                    continue;
                SD_LOG::appendBufferCSV("[%d/%d:%s][E:%02d/%02d/%zu]", p[i].addr, p[i].func, p[i].str.c_str(),
                             p[i].errs_uncorrected, p[i].errs_total, (p[i].len / 5) * 32);
                err_ttl += p[i].errs_total;err_un += p[i].errs_uncorrected;len += (p[i].len / 5) * 32;
            }
            SD_LOG::appendBufferCSV("\",");
            SD_LOG::appendBufferCSV("%d/%d,%.2f%%\n",err_un,err_ttl,((float)err_ttl/(float)len) * 100);
            break;
        }
        case 2:
        {
            SD_LOG::appendBufferCSV("%s,,,,,,,,,,,",l.time); //time,dir,cls,num,spd,pos,reg,rte,lat,lon,hex,
            break;
        }
    }
    if (l.type != 1 && !p[0].is_empty) {
        SD_LOG::appendBufferCSV("%3.1f,%5.2f,\"", r.rssi, r.fer);
        uint8_t err_un = 0,err_ttl = 0,len = 0;
        for (size_t i = 0; i < POCDAT_SIZE; i++) {
            if (p[i].is_empty)
                continue;
            SD_LOG::appendBufferCSV("[%d/%d:%s][E:%02d/%02d/%zu]", p[i].addr, p[i].func, p[i].str.c_str(),
                         p[i].errs_uncorrected, p[i].errs_total, (p[i].len / 5) * 32);
            err_un += p[i].errs_uncorrected;err_ttl += p[i].errs_total;len += (p[i].len / 5) * 32;
        }
        SD_LOG::appendBufferCSV("\",");
        SD_LOG::appendBufferCSV("%d/%d,%.2f%%\n",err_un,err_ttl,((float)err_ttl/(float)len) * 100);
    }
    SD_LOG::sendBufferCSV();
}