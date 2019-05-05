//
// Created by development on 05.01.19.
//

#ifndef ESP8266_WIFI_HELPER_H
#define ESP8266_WIFI_HELPER_H


#define PIN_POWER_CAL 4 // D15
#define PIN_LED 5 //

#define TIME_ZONE -6
#define US2MIN 60000000

#define SHOW_BLINK


#ifdef SHOW_BLINK
#define BLINK(x)  LED_Blink(x)
#else
#define BLINK(x)
#endif

//#define MYDEBUG

#ifdef MYDEBUG
#define MYDEBUG_CORE
#define DP(x)     Serial.print (x)
#define DPD(x)     Serial.print (x, DEC)
#define DPL(x)  Serial.println (x)
#else
#define DP(x)
#define DPD(x)
#define DPL(x)
#endif


//#define MYDEBUG_CORE

#ifdef MYDEBUG_CORE
#define CP(x)     Serial.print (x)
#define CPD(x)     Serial.print (x, DEC)
#define CPL(x)  Serial.println (x)
#else
#define CP(x)
#define CPD(x)
#define CPL(x)
#endif


struct rtcDataOauthStruct {
    uint32_t crc32;   // 4 bytes
    uint8_t status;
    char device_code[120];  // 1 byte,   5 in total
    char refresh_token[100];

};

struct rtcDataWakeupStruct {
    uint32_t crc32;   // 4 bytes
    uint8_t wakeup_count;
    uint16_t remaining_sleep_min;
};



static const int ERROR_STATE = -1;
static const int WAKE_UP_FROM_SLEEP = 1;
static const int CAL_WIFI_GET_CONFIG = 2;
static const int WIFI_INIT = 3;
static const int WIFI_INITIAL_STATE = 4; //request user code
static const int WIFI_AWAIT_CHALLENGE = 5;
static const int WIFI_CHECK_ACCESS_TOKEN=6;
static const int CAL_WAIT_READY = 7;
static const int CAL_PAINT_UPDATE = 8;
static const int CAL_PAINT_DONE = 9;
static const int ESP_START_SLEEP = 10;
static const int ESP_SEND_ERROR_MSG = 11;
static const int CAL_WIFI_GET_CONFIG_QUICK = 12;




#endif //ESP8266_WIFI_HELPER_H
