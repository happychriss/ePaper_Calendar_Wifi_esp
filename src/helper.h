//
// Created by development on 05.01.19.
//

#ifndef ESP8266_WIFI_HELPER_H
#define ESP8266_WIFI_HELPER_H



struct rtcDataStruct {
    uint32_t crc32;   // 4 bytes
    uint8_t status;
    char device_code[120];  // 1 byte,   5 in total
    char refresh_token[100];
};


static const int ERROR_STATE = -1;
static const int WIFI_INITIAL_STATE = 1; //request user code
static const int WIFI_AWAIT_CHALLENGE = 2;
static const int WIFI_CHECK_ACCESS_TOKEN=3;
static const int CAL_WAIT_READY = 4;
static const int CAL_PAINT_UPDATE = 5;
static const int CAL_PAINT_DONE = 6;

#define MYDEBUG

#ifdef MYDEBUG
#define DP(x)     Serial.print (x)
#define DPD(x)     Serial.print (x, DEC)
#define DPL(x)  Serial.println (x)
#else
#define DP(x)
#define DPD(x)
#define DPL(x)
#endif

#endif //ESP8266_WIFI_HELPER_H
