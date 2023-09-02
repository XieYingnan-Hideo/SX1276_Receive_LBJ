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
#include <ctime>
#include "sntp.h"
#include "ESPTelnet.h"
// definitions

#define WIFI_SSID       "GL-MT1300-b99"
#define WIFI_PASSWORD   "goodlife"

// variables

const char* time_zone = "CST-8";
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
struct tm time_info{};

ESPTelnet telnet;
IPAddress ip;
uint16_t  port = 23;


SX1276 radio = new Module(RADIO_CS_PIN, RADIO_DIO0_PIN, RADIO_RST_PIN, RADIO_DIO1_PIN);
// receiving packets requires connection
// to the module direct output pin
const int pin = RADIO_BUSY_PIN;

float rssi = 0;
float fer = 0;
// create Pager client instance using the FSK module
PagerClient pager(&radio);

uint64_t timer1 = 0;

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
bool isConnected() {
    return (WiFiClass::status() == WL_CONNECTED);
}
bool connectWiFi(const char* ssid, const char* password, int max_tries = 20, int pause = 500) {
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

/* ------------------------------------------------- */

// (optional) callback functions for telnet events
void onTelnetConnect(String ip) {
    Serial.print("- Telnet: ");
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
    telnet.print("> ");
}

void onTelnetDisconnect(String ip) {
    Serial.print("- Telnet: ");
    Serial.print(ip);
    Serial.println(" disconnected");
}

void onTelnetReconnect(String ip) {
    Serial.print("- Telnet: ");
    Serial.print(ip);
    Serial.println(" reconnected");
}

void onTelnetConnectionAttempt(String ip) {
    Serial.print("- Telnet: ");
    Serial.print(ip);
    Serial.println(" tried to connected");
}

void onTelnetInput(String str) {
    // checks for a certain command
    Serial.println("- Telnet: " + str);
    if (str == "ping") {
        telnet.println("- pong");
        Serial.println("- Telnet:- pong");
        // disconnect the client
    } else if (str == "bye") {
        telnet.println("- disconnecting you...");
        telnet.disconnectClient();
    }
    telnet.print("> ");
}


/* ------------------------------------------------- */

void setupTelnet() {
    // passing on functions for various telnet events
    telnet.onConnect(onTelnetConnect);
    telnet.onConnectionAttempt(onTelnetConnectionAttempt);
    telnet.onReconnect(onTelnetReconnect);
    telnet.onDisconnect(onTelnetDisconnect);
    telnet.onInputReceived(onTelnetInput);

    Serial.print("- Telnet: ");
    if (telnet.begin(port)) {
        Serial.println("running");
    } else {
        Serial.println("error.");
    }
}

/* ------------------------------------------------- */

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

void setup() {
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
//    Serial.print(F("[SX1276] Initializing ... "));
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
    // address of this "pager":     1234567
    state = pager.startReceive(pin, 1234000, 0xFFFF0);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("success!"));
    } else {
        Serial.print(F("failed, code "));
        Serial.println(state);
        while (true);
    }
    digitalWrite(BOARD_LED, LOW);

}

// LOOP

void loop() {
    if (millis()-timer1 >= 100)
        digitalWrite(BOARD_LED, LOW);

    if (isConnected() && !telnet.online){
        ip = WiFi.localIP();
        Serial.printf("WIFI Connection to %s established.",WIFI_SSID);
        Serial.print("- Telnet: "); Serial.print(ip); Serial.print(":"); Serial.println(port);
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
        PagerClient::pocsag_data pocdat[POCDAT_SIZE];

        Serial.print(F("[Pager] Received pager data, decoding ... "));

        // you can read the data as an Arduino String
        String str = {};
        unsigned int addr;
        uint32_t func;
        bool add = false;
        size_t clen = 0;
//        size_t len = 0;
//         int state = pager.readDataMod(str, 0, &addr, &func, &add, &clen);

        // you can also receive data as byte array
        /*
          byte byteArr[8];
          size_t numBytes = 0;
          int state = radio.receive(byteArr, &numBytes);
        */
        int state = pager.readDataMSA(pocdat,0);

        if (state == RADIOLIB_ERR_NONE) {
            Serial.printf("success!\n");
            digitalWrite(BOARD_LED, HIGH);
            timer1 = millis();

            for (auto & i : pocdat){
                if (i.is_empty)
                    continue;
                Serial.print(F("[Pager] Address:  "));
                Serial.printf("%d\t", i.addr);
                Serial.print(F("Function:  "));
                Serial.printf("%d\t", i.func);
                Serial.print(F("Data:  "));
                Serial.printf("%s\t", i.str.c_str());
                Serial.print(F("Errors:  "));
                Serial.printf("%d/%d\n",i.errs_uncorrected,i.errs_total);
                str = str + "  " + i.str;
            }

//            // print the received data
//            Serial.print(F("[Pager] Address:\t"));
//            Serial.println(addr);
//            Serial.print(F("[Pager] Function:\t"));
//            Serial.println(func);
//            if (!add){
//                Serial.print(F("[Pager] Data:\t"));
//                Serial.println(str);
//            } else {
//                Serial.print(F("[Pager] Data:\t"));
//                for (size_t i=0;i<clen;i++)
//                    Serial.print(str[i]);
//                Serial.print("\n");
//                Serial.print(F("[Pager] DataA:\t"));
//                for (size_t i=15;i<str.length();i++)
//                    Serial.print(str[i]);
//                Serial.print("\n");
//            }


            // print rssi
            Serial.print(F("[SX1276] RSSI:\t"));
            Serial.print(rssi);
            Serial.print(F(" dBm\t"));
            rssi = 0;

            // print FreqERR
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
            Serial.println("Too many errors.");
        }
        else {
            // some error occurred
            Serial.print(F("failed, code "));
            Serial.println(state);
        }
    }
}
