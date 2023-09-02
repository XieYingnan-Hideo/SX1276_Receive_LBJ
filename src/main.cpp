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

// include the library
#include "boards.h"
#include <RadioLib.h>
#include <WiFi.h>
#include "sntp.h"
#include "networks.h"
#include "iconv.h"
// definitions


// variables
SX1276 radio = new Module(RADIO_CS_PIN, RADIO_DIO0_PIN, RADIO_RST_PIN, RADIO_DIO1_PIN);
// receiving packets requires connection
// to the module direct output pin
const int pin = RADIO_BUSY_PIN;
float rssi = 0;
float fer = 0;
// create Pager client instance using the FSK module
PagerClient pager(&radio);
uint64_t timer1 = 0;
uint64_t timer2 = 0;
uint32_t ip_last = 0;

// functions

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
#endif

void dualPrintf(const char* fmt, ...){
    va_list args;
    va_start(args, fmt);
    Serial.printf(fmt,args);
    telnet.printf(fmt,args);
}
void dualPrint(const char* fmt){
    Serial.print(fmt);
    telnet.print(fmt);
}
void dualPrintln(const char* fmt){
    Serial.println(fmt);
    telnet.println(fmt);
}

// SETUP

void setup() {\
    timer2 = millis();
    initBoard();
    delay(1500);

    // Sync time.

    sntp_set_time_sync_notification_cb( timeAvailable );
    sntp_servermode_dhcp(1);
    configTzTime(time_zone, ntpServer1, ntpServer2);

    // initialize wireless network.
    Serial.printf("Connecting to %s ",WIFI_SSID);
    connectWiFi(WIFI_SSID,WIFI_PASSWORD,1); // usually max_tries = 25.
    if (isConnected()) {
        ip = WiFi.localIP();
        Serial.println();
        Serial.print("- Telnet: "); Serial.print(ip); Serial.print(":"); Serial.println(port);
        setupTelnet();
    } else {
        Serial.println();
        Serial.println("Error connecting to WiFi, Telnet startup skipped.");
    }

    // initialize SX1276 with default settings

    dualPrint("[SX1276] Initializing ... ");
    int state = radio.beginFSK(434.0, 4.8, 5.0, 12.5);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("success!"));
    } else {
        Serial.print(F("failed, code "));
        Serial.println(state);
        while (true);
    }

    state = radio.setGain(1);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("[SX1276] Gain set."));
    } else {
        Serial.print(F("failed, code "));
        Serial.println(state);
        while (true);
    }

    // initialize Pager client
    Serial.print(F("[Pager] Initializing ... "));
    // base (center) frequency:     821.2375 MHz + 0.005 FE
    // speed:                       1200 bps
    state = pager.begin(821.2375 + 0.005, 1200, false, 2500);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("success!"));
    } else {
        Serial.print(F("failed, code "));
        Serial.println(state);
        while (true);
    }

    // start receiving POCSAG messages
    Serial.print(F("[Pager] Starting to listen ... "));
    // address of this "pager":     12340XX
    state = pager.startReceive(pin, 1234000, 0xFFFF0);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("success."));
    } else {
        Serial.print(F("failed, code "));
        Serial.println(state);
        while (true);
    }

    digitalWrite(BOARD_LED, LOW);
    Serial.printf("Booting time %llu ms\n",millis()-timer2);
    timer2 = 0;

}

// LOOP

void loop() {
    if (ip_last != WiFi.localIP()){
        Serial.print("Local IP ");
        Serial.print(WiFi.localIP());
        Serial.print("\n");
    }
    ip_last = WiFi.localIP();


    if (millis()-timer1 >= 100)
        digitalWrite(BOARD_LED, LOW);

    if (isConnected() && !telnet.online){
        ip = WiFi.localIP();
        Serial.printf("WIFI Connection to %s established.\n",WIFI_SSID);
        Serial.print("[Telnet] "); Serial.print(ip); Serial.print(":"); Serial.println(port);
        setupTelnet();
    }

    telnet.loop();

    if (pager.gotSyncState()) {
        if (rssi == 0)
            rssi = radio.getRSSI(false, true);
        // todo：implement packet RSSI indicator based on average RSSI value.
        if (fer == 0)
            fer = radio.getFrequencyError();
    }

    // the number of batches to wait for
    // 2 batches will usually be enough to fit short and medium messages
    if (pager.available() >= 2) {
        timer2 = millis();
        PagerClient::pocsag_data pocdat[POCDAT_SIZE];
        struct lbj_data lbj;

        Serial.print(F("[Pager] Received pager data, decoding ... "));

        // you can read the data as an Arduino String
        String str = {};

        int state = pager.readDataMSA(pocdat,0);

        if (state == RADIOLIB_ERR_NONE) {
            Serial.printf("success.\n");
            digitalWrite(BOARD_LED, HIGH);
            timer1 = millis();

            for (auto & i : pocdat){
                if (i.is_empty)
                    continue;
                float ber = (float) i.errs_total / ((float) i.len * 4);
                Serial.printf("[Pager] %d/%d: %s  [ERR %d/%d/%zu, BER %.2f%%]\n", i.addr, i.func,
                       i.str.c_str(), i.errs_uncorrected, i.errs_total,i.len*4,ber * 100);
                str = str + "  " + i.str;
            }

            readDataLBJ(pocdat,&lbj);
            // Serial.printf("type %d \n",lbj.type);

            // print data in lbj format.
            if (lbj.type == 2){            // Time sync
                Serial.printf("[LBJ] Current Time %s \n",lbj.time);
            } else if (lbj.type == 1) {     // Additional
                if (lbj.direction == FUNCTION_UP)
                    Serial.printf("[LBJ] 方向：上行  ");
                else if (lbj.direction == FUNCTION_DOWN)
                    Serial.printf("[LBJ] 方向：下行  ");
                else
                    Serial.printf("[LBJ] 方向：%3d  ",lbj.direction);
                Serial.printf("车次：%s%s  速度：%6s KM/H  公里标：%8s KM\n" // 想办法把车次前面的空格去了...
                       "[LBJ] 机车编号：%s  线路：%s  ", lbj.lbj_class, lbj.train, lbj.speed, lbj.position,
                       lbj.loco, lbj.route);
                if (lbj.pos_lat_deg[1] && lbj.pos_lat_min[1])
                    printf("位置：%s°%2s′ ",lbj.pos_lat_deg,lbj.pos_lat_min);
                else
                    printf("位置：%s ",lbj.pos_lat);
                if (lbj.pos_lon_deg[1] && lbj.pos_lon_min[1])
                    printf("%s°%2s′\n",lbj.pos_lon_deg,lbj.pos_lon_min);
                else
                    printf("%s\n",lbj.pos_lon);

            } else if (lbj.type == 0) {      // Standard
                if (lbj.direction == FUNCTION_UP)
                    Serial.printf("[LBJ] 方向：上行  ");
                else if (lbj.direction == FUNCTION_DOWN)
                    Serial.printf("[LBJ] 方向：下行  ");
                else
                    Serial.printf("[LBJ] 方向：%3d  ",lbj.direction);
                Serial.printf("车次：%7s  速度：%6s KM/H  公里标：%8s KM\n",lbj.train,lbj.speed,lbj.position);
            }

            // print rssi
            Serial.print(F("[SX1276] RSSI:\t"));
            Serial.print(rssi);
            Serial.print(F(" dBm\t"));
            rssi = 0;

            // print Frequency Error
            Serial.print(F("Frequency Error:\t"));
            Serial.print(fer);
            Serial.println(F(" Hz"));
            fer = 0;

#ifdef HAS_DISPLAY
            if (u8g2) {
                u8g2->clearBuffer();
                u8g2->drawStr(0, 12, "Received OK!");
                // u8g2->drawStr(0, 26, str.c_str());
                pword(str.c_str(), 0, 26);
                u8g2->sendBuffer();
            }
#endif
//            if (millis()-timer1 >= 100)
//                digitalWrite(BOARD_LED, LOW);

        } else if (state == RADIOLIB_ERR_MSG_CORRUPT) {
            Serial.printf("failed.\n");
            Serial.println("[Pager] Too many errors.");
        }
        else {
            // some error occurred
            Serial.print(F("[Pager] failed, code "));
            Serial.println(state);
        }
        Serial.printf("[Pager] Processing time %llu ms\n",millis()-timer2);
        timer2 = 0;
    }
}
