/*
   RadioLib Pager (POCSAG) Receive Example

   This example shows how to receive FSK packets without using
   SX127x packet engine.

   This example receives POCSAG messages using SX1278's
   FSK modem in direct mode.

   Other modules that can be used to receive POCSAG:
    - SX127x/RFM9x
    - RF69
    - SX1231
    - CC1101
    - Si443x/RFM2x

   For default module settings, see the wiki page
   https://github.com/jgromes/RadioLib/wiki/Default-configuration#sx127xrfm9x---lora-modem

   For full API reference, see the GitHub Pages
   https://jgromes.github.io/RadioLib/
*/
#pragma execution_character_set("utf-8")
// include the library
#include "boards.h"
#include <RadioLib.h>
#include <WiFi.h>
#include "esp_sntp.h"
#include "networks.h"
#include "sdlog.h"
#include "customfont.h"

//region Variables
SX1276 radio = new Module(RADIO_CS_PIN, RADIO_DIO0_PIN, RADIO_RST_PIN, RADIO_DIO1_PIN);
// receiving packets requires connection
// to the module direct output pin
const int pin = RADIO_BUSY_PIN;
//float rssi = 0;
//float fer = 0;
// create Pager client instance using the FSK module
PagerClient pager(&radio);
// todo: millis返回值每50天就会溢出，考虑新建一个变量，返回值每归零一次就加1，并且以此处理使用计时器的函数。
uint64_t timer1 = 0;
uint64_t timer2 = 0;
uint64_t timer3 = 0;
uint64_t timer4 = 0;
uint64_t net_timer = 0;
uint32_t ip_last = 0;
bool is_startline = true;
bool exec_init_f80 = false;
bool agc_triggered = false;
bool low_volt_warned = false;
bool give_tel_rssi = false;
bool give_tel_gain = false;
bool no_wifi = false;
SD_LOG sd1;
struct rx_info rxInfo;
//endregion

//region Functions

#ifdef HAS_DISPLAY

void pword(const char *msg, int xloc, int yloc) {
    int dspW = u8g2->getDisplayWidth();
    int strW = 0;
    char glyph[2];
    glyph[1] = 0;
    for (const char *ptr = msg; *ptr; *ptr++) {
        glyph[0] = *ptr;
        strW += u8g2->getStrWidth(glyph);
        ++strW;
        if (xloc + strW > dspW) {
            int sxloc = xloc;
            while (msg < ptr) {
                glyph[0] = *msg++;
                xloc += u8g2->drawStr(xloc, yloc, glyph);
            }
            strW -= xloc - sxloc;
            yloc += u8g2->getMaxCharHeight();
            xloc = 0;
        }
    }
    while (*msg) {
        glyph[0] = *msg++;
        xloc += u8g2->drawStr(xloc, yloc, glyph);
    }
}

void showInitComp() {
    u8g2->clearBuffer();
    u8g2->setFont(u8g2_font_squeezed_b7_tr);
    // bottom (0,56,128,8)
    String ipa = WiFi.localIP().toString();
    u8g2->drawStr(0, 64, ipa.c_str());
    if (have_sd && WiFiClass::status() == WL_CONNECTED)
        u8g2->drawStr(83, 64, "D");
    else if (have_sd)
        u8g2->drawStr(83, 64, "L");
    else if (WiFiClass::status() == WL_CONNECTED)
        u8g2->drawStr(83, 64, "N");
    char buffer[32];
    sprintf(buffer, "%2d", ets_get_cpu_frequency() / 10);
    u8g2->drawStr(91, 64, buffer);
    sprintf(buffer, "%1.2f", battery.readVoltage() * 2);
    u8g2->drawStr(105, 64, buffer);
    // top (0,0,128,8)
    if (!getLocalTime(&time_info, 0))
        u8g2->drawStr(0, 7, "NO SNTP");
    else {
        sprintf(buffer, "%d-%02d-%02d %02d:%02d", time_info.tm_year + 1900, time_info.tm_mon + 1, time_info.tm_mday,
                time_info.tm_hour, time_info.tm_min);
        u8g2->drawStr(0, 7, buffer);
    }
    u8g2->sendBuffer();
}

void updateInfo() {
    // update top
    char buffer[32];
    u8g2->setDrawColor(0);
    u8g2->setFont(u8g2_font_squeezed_b7_tr);
    u8g2->drawBox(0, 0, 97, 8);
    u8g2->setDrawColor(1);
    if (!getLocalTime(&time_info, 0))
        u8g2->drawStr(0, 7, "NO SNTP");
    else {
        sprintf(buffer, "%d-%02d-%02d %02d:%02d", time_info.tm_year + 1900, time_info.tm_mon + 1, time_info.tm_mday,
                time_info.tm_hour, time_info.tm_min);
        u8g2->drawStr(0, 7, buffer);
    }
    // update bottom
    u8g2->setDrawColor(0);
    u8g2->drawBox(0, 56, 128, 8);
    u8g2->setDrawColor(1);
    if (!no_wifi) {
        String ipa = WiFi.localIP().toString();
        u8g2->drawStr(0, 64, ipa.c_str());
    } else
        u8g2->drawStr(0, 64, "WIFI OFF");
    if (have_sd && WiFiClass::status() == WL_CONNECTED)
        u8g2->drawStr(83, 64, "D");
    else if (have_sd)
        u8g2->drawStr(83, 64, "L");
    else if (WiFiClass::status() == WL_CONNECTED)
        u8g2->drawStr(83, 64, "N");
    sprintf(buffer, "%2d", ets_get_cpu_frequency() / 10);
    u8g2->drawStr(91, 64, buffer);
    voltage = battery.readVoltage() * 2;
    sprintf(buffer, "%1.2f", voltage); // todo: Implement average voltage reading.
    if (voltage < 3.15 && !low_volt_warned) {
        Serial.printf("Warning! Low Voltage detected, %1.2fV\n", voltage);
        sd1.append("低压警告，电池电压%1.2fV\n", voltage);
        low_volt_warned = true;
    }
    u8g2->drawStr(105, 64, buffer);
    u8g2->sendBuffer();
}

void showLBJ0(const struct lbj_data &l) {
    // box y 9->55
    char buffer[128];
    u8g2->setDrawColor(0);
    u8g2->drawBox(0, 8, 128, 48);
    u8g2->setDrawColor(1);
    u8g2->setFont(u8g2_font_wqy15_t_custom);
    if (l.direction == FUNCTION_UP) {
        sprintf(buffer, "车  次 %s 上行", l.train);
    } else if (l.direction == FUNCTION_DOWN)
        sprintf(buffer, "车  次 %s 下行", l.train);
    else
        sprintf(buffer, "车  次 %s %d", l.train, l.direction);
    u8g2->drawUTF8(0, 21, buffer);
    sprintf(buffer, "速  度  %s KM/H", l.speed);
    u8g2->drawUTF8(0, 37, buffer);
    sprintf(buffer, "公里标 %s KM", l.position);
    u8g2->drawUTF8(0, 53, buffer);
    // draw RSSI
    u8g2->setDrawColor(0);
    u8g2->drawBox(98, 0, 30, 8);
    u8g2->setDrawColor(1);
    u8g2->setFont(u8g2_font_squeezed_b7_tr);
    sprintf(buffer, "%3.1f", rxInfo.rssi);
    u8g2->drawStr(99, 7, buffer);
    u8g2->sendBuffer();
}

void showLBJ1(const struct lbj_data &l) {
    char buffer[128];
    u8g2->setDrawColor(0);
    u8g2->drawBox(0, 8, 128, 48);
    u8g2->setDrawColor(1);
    u8g2->setFont(u8g2_font_wqy12_t_gb2312a);
    // line 1
    sprintf(buffer, "车:%s%s", l.lbj_class, l.train);
    u8g2->drawUTF8(0, 19, buffer);
    sprintf(buffer, "速:%sKM/H", l.speed);
    u8g2->drawUTF8(68, 19, buffer);
    // line 2
    sprintf(buffer, "线:%s", l.route_utf8);
    u8g2->drawUTF8(0, 31, buffer);
    u8g2->drawBox(67, 21, 13, 12);
    u8g2->setDrawColor(0);
    if (l.direction == FUNCTION_UP)
        u8g2->drawUTF8(68, 31, "上");
    else if (l.direction == FUNCTION_DOWN)
        u8g2->drawUTF8(68, 31, "下");
    else {
        printf(buffer, "%d", l.direction);
        u8g2->drawUTF8(68, 31, buffer);
    }
    u8g2->setDrawColor(1);
    sprintf(buffer, "%sK", l.position);
    u8g2->drawUTF8(86, 31, buffer);
    // line 3
    sprintf(buffer, "号:%s", l.loco);
    u8g2->drawUTF8(0, 43, buffer);
    if (l.loco_type.length())
        u8g2->drawUTF8(72, 43, l.loco_type.c_str());
    // line 4
    String pos;
    if (l.pos_lat_deg[1] && l.pos_lat_min[1]) {
        sprintf(buffer, "%s°%s'", l.pos_lat_deg, l.pos_lat_min);
        pos += String(buffer);
    } else {
        sprintf(buffer, "%s ", l.pos_lat);
        pos += String(buffer);
    }
    if (l.pos_lon_deg[1] && l.pos_lon_min[1]) {
        sprintf(buffer, "%s°%s'", l.pos_lon_deg, l.pos_lon_min);
        pos += String(buffer);
    } else {
        sprintf(buffer, "%s ", l.pos_lon);
        pos += String(buffer);
    }
//    sprintf(buffer,"%s°%s'%s°%s'",l.pos_lat_deg,l.pos_lat_min,l.pos_lon_deg,l.pos_lon_min);
    u8g2->drawUTF8(0, 54, pos.c_str());
    // draw RSSI
    u8g2->setDrawColor(0);
    u8g2->drawBox(98, 0, 30, 8);
    u8g2->setDrawColor(1);
    u8g2->setFont(u8g2_font_squeezed_b7_tr);
    sprintf(buffer, "%3.1f", rxInfo.rssi);
    u8g2->drawStr(99, 7, buffer);
    u8g2->sendBuffer();
}

void showLBJ2(const struct lbj_data &l) {
    char buffer[128];
    u8g2->setDrawColor(0);
    u8g2->drawBox(0, 8, 128, 48);
    u8g2->setDrawColor(1);
    u8g2->setFont(u8g2_font_wqy15_t_custom);
    sprintf(buffer, "当前时间 %s ", l.time);
    u8g2->drawUTF8(0, 21, buffer);
    // draw RSSI
    u8g2->setDrawColor(0);
    u8g2->drawBox(98, 0, 30, 8);
    u8g2->setDrawColor(1);
    u8g2->setFont(u8g2_font_squeezed_b7_tr);
    sprintf(buffer, "%3.1f", rxInfo.rssi);
    u8g2->drawStr(99, 7, buffer);
    u8g2->sendBuffer();
}

#endif

void dualPrintf(bool time_stamp, const char *format, ...) { // Generated by ChatGPT.
    char buffer[256]; // 创建一个足够大的缓冲区来容纳格式化后的字符串
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args); // 格式化字符串
    va_end(args);

    // 输出到 Serial
    Serial.print(buffer);

    // 输出到 Telnet
    if (telnet.online) { // code from Multimon-NG unixinput.c 还得是multimon-ng，chatGPT写了四五个版本都没解决。
        if (is_startline) {
            telnet.print("\r> ");
            if (time_stamp && getLocalTime(&time_info, 0))
                telnet.printf("\r%d-%02d-%02d %02d:%02d:%02d > ", time_info.tm_year + 1900, time_info.tm_mon + 1,
                              time_info.tm_mday, time_info.tm_hour, time_info.tm_min, time_info.tm_sec);
            is_startline = false;
        }
        telnet.print(buffer);
        if (nullptr != strchr(buffer, '\n')) {
            is_startline = true;
            telnet.print("\r< ");
        }
    }
}

void dualPrint(const char *fmt) {
    Serial.print(fmt);
    telnet.print(fmt);
}

void dualPrintln(const char *fmt) {
    Serial.println(fmt);
    telnet.println(fmt);
}

void LBJTEST() {
    PagerClient::pocsag_data pocdat[16];
    pocdat[0].str = "37012";
    pocdat[0].addr = 1234000;
    pocdat[0].func = 1;
    pocdat[0].is_empty = false;
    pocdat[0].len = 15;
    pocdat[1].str = "20202350018530U)*9UU*6 (-(202011719040139058291000";
    pocdat[1].addr = 1234002;
    pocdat[1].func = 1;
    pocdat[1].is_empty = false;
    pocdat[1].len = 0;
//    Serial.println("[LBJ] 测试输出 机车编号 位置 XX°XX′XX″ ");
//    dualPrintf(false,"[LBJ] 测试输出 机车编号 位置 XX°XX′XX″ \n");
    struct lbj_data lbj;

    readDataLBJ(pocdat, &lbj);
    printDataSerial(pocdat, lbj, rxInfo);
    appendDataLog(pocdat,lbj,rxInfo);
    printDataTelnet(pocdat, lbj, rxInfo);
    rxInfo.rssi = 0;
    rxInfo.fer = 0;

}

int initPager() {// initialize SX1276 with default settings

    int state = radio.beginFSK(434.0, 4.8, 5.0, 12.5);
    RADIOLIB_ASSERT(state)

    state = radio.setGain(1);
    RADIOLIB_ASSERT(state)

    // initialize Pager client
    // Serial.print(F("[Pager] Initializing ... "));
    // base (center) frequency: 821.2375 MHz + 0.005 FE
    // speed:                   1200 bps
    state = pager.begin(821.2375 + 0.005, 1200, false, 2500);
    RADIOLIB_ASSERT(state)

    // start receiving POCSAG messages
    // Serial.print(F("[Pager] Starting to listen ... "));
    // address of this "pager": 12340XX
    state = pager.startReceive(pin, 1234000, 0xFFFF0);
    //TODO Enhancement: try to keep a open address filter, we might find something unknown.
    RADIOLIB_ASSERT(state)

    return state;
}
//endregion

// SETUP
void setup() {
    timer2 = millis();
    initBoard();
    sd1.setFS(SD);
    delay(150);

    if (have_sd) {
        sd1.begin("/LOGTEST");
        sd1.beginCSV("/CSVTEST");
        sd1.append("电池电压 %1.2fV\n", battery.readVoltage() * 2);
    }
    // Sync time.

    sntp_set_time_sync_notification_cb(timeAvailable);
    sntp_servermode_dhcp(1);
    configTzTime(time_zone, ntpServer1, ntpServer2);

    if (u8g2) {
        showInitComp();
        u8g2->setFont(u8g2_font_wqy12_t_gb2312a);
        u8g2->setCursor(0, 52);
        u8g2->println("Initializing...");
        u8g2->sendBuffer();
    }

    // initialize wireless network.
    Serial.printf("Connecting to %s ", WIFI_SSID);
    connectWiFi(WIFI_SSID, WIFI_PASSWORD, 1); // usually max_tries = 25.
    if (isConnected()) {
        ip = WiFi.localIP();
        Serial.println();
        Serial.print("[Telnet] ");
        Serial.print(ip);
        Serial.print(":");
        Serial.println(port);
        setupTelnet(); // todo: find another library / modify the code to support multiple client connection.
    } else {
        Serial.println();
        Serial.println("Error connecting to WiFi, Telnet startup skipped.");
    }

    // Initialize SX1276
    dualPrint("[SX1276] Initializing ... ");
    int state = initPager();
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("success."));
    } else {
        Serial.print(F("failed, code "));
        Serial.println(state);
        while (true);
    }

//    if(WiFi.getSleep())
//        Serial.println("WIFI Sleep enabled.");

    digitalWrite(BOARD_LED, LED_OFF);
    Serial.printf("Booting time %llu ms\n", millis() - timer2);
    sd1.append("启动用时 %llu ms\n", millis() - timer2);
    timer2 = 0;

    if (u8g2) {
        u8g2->setDrawColor(0);
        u8g2->drawBox(0, 42, 128, 14);
        u8g2->setDrawColor(1);
        u8g2->drawStr(0, 52, "Listening...");
        u8g2->sendBuffer();
        Serial.printf("Mem left: %d Bytes\n", esp_get_free_heap_size());
    }

    // test stuff
//    LBJTEST();
//     Serial.printf("CPU FREQ %d MHz\n",ets_get_cpu_frequency());

}

// Loop functions
void handleTelnetCall() {
    if (give_tel_rssi) {
        telnet.printf("> RSSI %3.2f dBm.\n", radio.getRSSI(false, true));
        give_tel_rssi = false;
        telnet.print("< ");
    }
    if (give_tel_gain) {
        telnet.printf("> Gain Pos %d \n", radio.getGain());
        give_tel_gain = false;
        telnet.print("< ");
    }
}

void handleSync() {
    if (pager.gotSyncState()) {
//        sd1.append("[PGR][DEBUG] SYNC DETECTED.\n");
        // INFO: This function can not work, calling agc here stops receiving.
        //    if (radio.getRSSI() >= -60.0 && !agc_triggered){
        //        Serial.println("[SX1276][D] AGC TRIGGERED.");
        //        radio.startAGC();
        //        Serial.printf("[SX1276] AGC Triggered. Current Gain Pos %d\n",radio.getGain());
        //        agc_triggered = true;
        //    }
        if (rxInfo.cnt < 5 && (rxInfo.timer == 0 || micros() - rxInfo.timer > 11000 || micros() - rxInfo.timer < 0)) {
            // It seems the micros will overflow, causing the program to stuck here. （真的吗...）
            float rssi = radio.getRSSI(false, true);
            // if (rssi >= -80.0 && !agc_triggered){
            //     radio.startAGC();
            //     Serial.printf("[SX1276] AGC Triggered. Current Gain Pos %d\n",radio.getGain());
            //     agc_triggered = true;
            // }
            rxInfo.timer = micros();
            rxInfo.rssi += rssi; // decreased number of rssi samples due to READ-DATA task running on spi bus.
            // Serial.printf("[D] RXI %.2f\n",rxInfo.rssi);
            rxInfo.cnt++;
        }
        if (rxInfo.fer == 0)
            rxInfo.fer = radio.getFrequencyError();
    }
}


void handleTelnet() {
    if (isConnected() && !telnet.online) {
        ip = WiFi.localIP();
        Serial.printf("WIFI Connection to %s established.\n", WIFI_SSID);
        Serial.print("[Telnet] ");
        Serial.print(ip);
        Serial.print(":");
        Serial.println(port);
        setupTelnet();
    }

    telnet.loop();
}

void checkNetwork() {
    if (isConnected() && net_timer != 0)
        net_timer = 0;
    else if (!isConnected() && net_timer == 0)
        net_timer = millis();

    if (!isConnected() && millis() - net_timer > NETWORK_TIMEOUT) { // 暂定解决方案：超30分钟断wifi
        WiFi.disconnect();
        WiFiClass::mode(WIFI_OFF);
        no_wifi = true;
    }

    if (ip_last != WiFi.localIP()) {
        Serial.print("Local IP ");
        Serial.print(WiFi.localIP());
        Serial.print("\n");
    }
    ip_last = WiFi.localIP();
}

// LOOP
void loop() {
    checkNetwork();
    handleTelnetCall();

    if (millis() > 60000 && timer1 == 0 && !exec_init_f80) // lower down frequency 60 sec after startup and idle.
    {
        if (isConnected())
            setCpuFrequencyMhz(80);
        else {
            WiFiClass::mode(WIFI_OFF);
            setCpuFrequencyMhz(80);
            WiFiClass::mode(WIFI_MODE_STA);
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        }
        exec_init_f80 = true;
    }

    if (timer3 == 0) {
        timer3 = millis();
    } else if (millis() - timer3 > 3000) { // Set to 3000 to reduce interference.
        updateInfo();
        timer3 = millis();
    }

   // if (millis()%5000 == 0){
   //     sd1.append("[D] 当前运行时间 %lu ms.\n",millis());
   //     sd1.append("[D] 测试输出：\n");
   //     LBJTEST();
   // }

    if (millis() - timer1 >= 200 && timer1) {
        // Serial.printf("LED LOW [%llu]\n", millis() - timer1);
        digitalWrite(BOARD_LED, LED_OFF);
        timer1 = 0;
        changeCpuFreq(240);
    }

    if (millis() - timer4 >= 60000 && timer4 != 0 && ets_get_cpu_frequency() != 80)
//        setCpuFrequencyMhz(80);
        changeCpuFreq(80);

    handleTelnet();
    handleSync();

    // the number of batches to wait for
    // 2 batches will usually be enough to fit short and medium messages
    if (pager.available() >= 2) { // todo add session timeout exception to prevent stuck here.
        // Serial.println("[PHY-LAYER][D] AVAILABLE > 2.");
//        sd1.append("[PHY-LAYER][D] AVAILABLE > 2.\n");
        rxInfo.rssi = rxInfo.rssi / (float) rxInfo.cnt;
        rxInfo.cnt = 0;
        rxInfo.timer = 0;
        timer2 = millis();
        timer4 = millis();
        setCpuFrequencyMhz(240);
//        Serial.printf("CPU FREQ TO %d MHz\n",ets_get_cpu_frequency());

        PagerClient::pocsag_data pocdat[POCDAT_SIZE];
        struct lbj_data lbj;

        Serial.println(F("[Pager] Received pager data, decoding ... "));

        // you can read the data as an Arduino String
        String str = {};

        int state = pager.readDataMSA(pocdat, 0);

        if (state == RADIOLIB_ERR_NONE) {
//            Serial.printf("success.\n");
            digitalWrite(BOARD_LED, LED_ON);
            timer1 = millis();

            for (auto &i: pocdat) {
                if (i.is_empty)
                    continue;
                Serial.printf("[D-pDATA] %d/%d: %s\n", i.addr, i.func, i.str.c_str());
                str = str + "  " + i.str;
            }

            // Serial.printf("decode complete.[%llu]", millis() - timer2);
            readDataLBJ(pocdat, &lbj);
            // Serial.printf("Read complete.[%llu]", millis() - timer2);

            printDataSerial(pocdat, lbj, rxInfo);
            // Serial.printf("SPRINT complete.[%llu]", millis() - timer2);

            sd1.disableSizeCheck();
            appendDataLog(pocdat, lbj, rxInfo);
            // Serial.printf("sdprint complete.[%llu]", millis() - timer2);
            appendDataCSV(pocdat, lbj, rxInfo);
            // Serial.printf("csvprint complete.[%llu]", millis() - timer2);
            sd1.enableSizeCheck();

            printDataTelnet(pocdat, lbj, rxInfo);
            // Serial.printf("telprint complete.[%llu]", millis() - timer2);
            // Serial.printf("type %d \n",lbj.type);

//            // print rssi
//            dualPrintf(true,"[SX1276] RSSI:  ");
//            dualPrintf(true,"%.2f",rssi);
//            dualPrintf(true," dBm  ");
//            sd1.append("[SX1276] RSSI: %.2f dBm    Frequency Error: %.2f Hz \n",rssi,fer);
//            rxInfo.rssi = 0;
//
//            // print Frequency Error
//            dualPrintf(true,"Frequency Error:  ");
//            dualPrintf(true,"%.2f",fer);
//            dualPrintf(true," Hz\r\n");
            // rxInfo.fer = 0;

#ifdef HAS_DISPLAY
            if (u8g2) {
                if (lbj.type == 0)
                    showLBJ0(lbj);
                else if (lbj.type == 1) {
                    showLBJ1(lbj);
                } else if (lbj.type == 2) {
                    showLBJ2(lbj);
                }
                // Serial.printf("Complete u8g2 [%llu]\n", millis() - timer2);
            }
#endif

        } else if (state == RADIOLIB_ERR_MSG_CORRUPT) {
//            Serial.printf("failed.\n");
//            Serial.println("[Pager] Reception failed, too many errors.");
            dualPrintf(true, "[Pager] Reception failed, too many errors. \n");
//            sd1.append("[Pager] Reception failed, too many errors. \n");
        } else {
            // some error occurred
            sd1.append("[Pager] Reception failed, code %d \n", state);
            dualPrintf(true, "[Pager] Reception failed, code %d \n", state);
        }
        Serial.printf("[Pager] Processing time %llu ms.\n", millis() - timer2);
        timer2 = 0;
        rxInfo.rssi = 0;
        rxInfo.fer = 0;
        //    if (agc_triggered) {
        //        radio.setGain(1);
        //        agc_triggered = false;
        //    }
    }
}
// END OF FILE.