# Pager_Receive
A LBJ Message Receiver Based on TTGO LoRa 32 v1.6.1 (SX1276)

The Pager_Receive project is modified from [RadioLib](https://github.com/jgromes/RadioLib)'s Pager_Receive.ino, based on the TTGO LoRa 32 v1.6.1 dev board, aims to provide an alternative to the expensive programmable pagers commonly used in receiving the Chinese Railway Train Proximity Alarm messages (often transmitted by on board LBJ systems).

## Notice
This project is a prototype, a jump start, and currently WIP.
It involves migrating and modifying multiple (possibly conflicting) libraries and codes.
As I am a rookie, the firmware is barely functional.
Consequently, the project is highly experimental and riddled with numerous issues.
Please use it at your own risk.

## Features
- Shows formatted LBJ messages on OLED display.
- Save Received messages to TF card in plain text and CSV format.
- Host Telnet server and send formatted messages to client.
- BCH error correction migrated from MMDVM_HS_Hat's [BCH3121.cpp](https://github.com/phl0/POCSAG_HS/blob/master/BCH3121.cpp)
- Support long messages contains routes, locomotive numbers and geographic coordinates (NOT FULLY DECODED).

## Known issues
- Memory overflows and trigger reboot occasionally.
- No inputs in any form (buttons, touch screens, etc.) except Telnet. 
- No multi-client support for Telnet service.
- No RTC module, real time acquired from NTP server via WI-FI.

## Details
### 1. About LBJ long message
It does not appear on TB/T 3504-2018 standard. 
I received this kind of message using SDR while listening to 821.2375MHz. 
The format of these messages is based purely on guess, currently identified information in one typical long LBJ message is listed in the following chart.  
(WIP)

## Libraries and codes used/referenced
[//]: # (todo: add links)
- RadioLib
- U8G2
- ESP32-Arduino
- ESP32AnalogRead (for battery voltage checking)
- BCH3121.cpp/.h
- (WIP)