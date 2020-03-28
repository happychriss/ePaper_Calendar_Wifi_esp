//
// Created by development on 05.01.19.
//

#ifndef ESP8266_WIFI_MAIN_ESP8266_WIFI_H
#define ESP8266_WIFI_MAIN_ESP8266_WIFI_H



// Commands to Calendar
#define CMD_READ_CALENDAR 1
#define CMD_SHUTDOWN_CALENDAR 2
#define CMD_SHOW_USERCODE_CALENDAR 3
#define CMD_SHOW_ERROR_MSG 4
#define CMD_SEND_WIFI 5
#define CMD_AWAKE_TEST 6


// If the calendar wants to get more data
#define CALENDAR_READY 1
#define CALENDAR_STATUS_MORE  2
#define CALENDAR_STATUS_READY  3



/*** How long to sleep **/
#define WAKE_UP_HOUR 2
#define WAKE_UP_MIN 12
// #define CYCLE_SLEEP_HOURS 3 //wake up every x hours (divided by warp-factor)
#define WARP_FACTOR 1 //testing faster wakeup

#define US2MIN 60000000
#define MAX_SLEEP_MIN 70 // minuntes as max-sleep (*US2MIN = 60000000)

/**** Debug options *******/
//#define MYDEBUG
//#define MYDEBUG_CORE
//#define WAIT_SERIAL
#define SHOW_BLINK

#define TIME_ZONE -6
#define PIN_POWER_CAL 4 // D15
#define PIN_LED 5 //


#ifdef SHOW_BLINK
#define BLINK(x)  LED_Blink(x)
#else
#define BLINK(x)
#endif


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
    char device_code[1024];  // 1 byte,   5 in total, old 120
    char refresh_token[1024]; // Refresh tokens: 512 bytes, old 100

};

struct rtcDataWakeupStruct {
    uint32_t crc32;   // 4 bytes
    uint8_t wakeup_count;
    uint16_t remaining_sleep_min;
    bool b_wake_up_after_error;
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
static const int CAL_QUICK_INIT = 12;
static const int CAL_WAKEUP = 13;






#endif //ESP8266_WIFI_MAIN_ESP8266_WIFI_H
