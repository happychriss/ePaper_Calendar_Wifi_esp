#define WAKE_UP_HOUR 2
#define WAKE_UP_MIN 10

#include <ESP8266WiFi.h>
//#include <WiFiClientSecureBearSSL.h>
#include <WiFiClientSecureAxTLS.h> // force use of AxTLS (BearSSL is now default)
#include "string"
#include "oauth.h"
#include "helper.h"
#include "time.h"
#include <SoftwareSerial.h>
#include "EEPROM.h"
#include "cal_comm.h"

// source: https://github.com/markszabo/IRremoteESP8266/blob/master/src/IRutils.cpp#L48
String uint64ToString(uint64_t input) {
    String result = "";
    uint8_t base = 10;

    do {
        char c = input % base;
        input /= base;

        if (c < 10)
            c += '0';
        else
            c += 'A' - 10;
        result = c + result;
    } while (input);
    return result;
}


//receivePin, transmitPin,
SoftwareSerial swSer(12, 13, false, 256); //STM32: Weiß muss an RX, Grun an TX


const char *host = "www.googleapis.com";

char my_ssid[50] = {0};
char my_pwd[50] = {0};
char *error_msg;
char str_time[22];
char *my_error_msg;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored  "-Wdeprecated-declarations"


WiFiClientSecure client;
tm global_time;


#pragma GCC diagnostic pop


rtcDataOauthStruct rtcOAuth;
rtcDataWakeupStruct rtcWakeUp;

char global_access_token[150] = "ya29.GluPBv80YVsTmc5uCIDt5AoEDbdoP-srm1zk7zzfF-1qRCmOHwcTlXsIgGGKJmNufn5XaRSAdh4B-T4gkizphT20SFW54IlyMZD7wepBC-2OW9W1elL8Z6swZTcO\0";
// char global_access_token[150];

// Set global variable attributes.
uint8_t global_status;
bool b_reset_authorization = false;


#define PIN_POWER_CAL 15 // D15

// Commands to Calendar
#define CMD_READ_CALENDAR 1
#define CMD_SHUTDOWN_CALENDAR 2
#define CMD_SHOW_USERCODE_CALENDAR 3
#define CMD_SHOW_ERROR_MSG 4


// If the calendar wants to get more data
#define CALENDAR_READY 1
#define CALANDAR_STATUS_MORE  2
#define CALENDAR_STATUS_DONE  3


void setup() {
    pinMode(PIN_POWER_CAL, OUTPUT);
    digitalWrite(PIN_POWER_CAL, LOW);
    swSer.begin(9600);
    EEPROM.begin(250);

//#define WAIT_SERIAL
    Serial.begin(115200);
#ifdef WAIT_SERIAL

    int c;
    Serial.println("max deep sleep: " + uint64ToString(ESP.deepSleepMax()));
    DPL("Key to start, 'x' to reset authentication");
    c = SerialKeyWait();
    DP("Typed:");
    DPL(c);
    if (c == 120) { //x
        b_reset_authorization = true;
    }
#endif
    global_status = WAKE_UP_FROM_SLEEP;
}


void loop() {



    DP("************** State:");DPD(global_status);DPL(" ****************");

    String request;

    switch (global_status) {

        case WAKE_UP_FROM_SLEEP:
            if (RTC_WakeUpRead()) {

                if (rtcWakeUp.wakeup_count > 1) {
                    DP("DeepSleep for Max-Time with count:");DPL(rtcWakeUp.wakeup_count);
                    rtcWakeUp.wakeup_count = rtcWakeUp.wakeup_count - 1;
                    RTC_WakeUpWrite();
                    ESP.deepSleep(ESP.deepSleepMax());
                }

                if (rtcWakeUp.wakeup_count == 1) {
                    rtcWakeUp.wakeup_count = rtcWakeUp.wakeup_count - 1;
                    RTC_WakeUpWrite();
                    DP("DeepSleep for min:");DPL(rtcWakeUp.remaining_sleep_min);
                    ESP.deepSleep(rtcWakeUp.remaining_sleep_min * 60000);
                }

                if (rtcWakeUp.wakeup_count == 0) {
                    global_status = CAL_WAIT_READY;
                    DPL("DeepSleep: DONE - Start Calendar Flow");
                }
            } else {
                global_status = CAL_WAIT_READY;
                DPL("DeepSleep: No Valid RTC found - Start Calendar Flow");
            }


            break;

        case CAL_WAIT_READY:
            digitalWrite(PIN_POWER_CAL, HIGH);
            delay(1000);
            DPL("Waiting for calender to be ready...");

            while (WaitForCalendarStatus() != CALENDAR_READY) {}

            global_status = CAL_WIFI_GET_CONFIG;
            break;


        case CAL_WIFI_GET_CONFIG:

            DPL("Getting Wifi Config from Calendar..");

            ReadFromCalendar(my_ssid);
            DP("SSID:");
            DPL(my_ssid);

            ReadFromCalendar(my_pwd);
            DP("PWD: ");
            DPL(my_pwd);

            global_status = WIFI_INIT;

            break;

        case WIFI_INIT:
            DPL("Init Wifi");

            if (SetupMyWifi(my_ssid, my_pwd)) {
             error_msg=strdup(" - WIFI Connection ERROR!");
             global_status=ESP_SEND_ERROR_MSG;
             break;
            }

            SetupTimeSNTP(&global_time);

            if (RTC_OAuthRead()) {
                DP("Status: ");
                DPL(rtcOAuth.status);
                DP("Device-Code: ");
                DPL(rtcOAuth.device_code);
                DP("Refresh-Token: ");
                DPL(rtcOAuth.refresh_token);
                DP("Wakeup_Count: ");

                global_access_token[0] = 0;

                if (b_reset_authorization) {
                    DPL("*Reset Google User access - set initial state*");
                    global_status = WIFI_INITIAL_STATE;
                } else {
                    global_status = rtcOAuth.status;
                }

            } else {
                DPL("** Invalid RTC Status - Reauth");;
                global_status = WIFI_INITIAL_STATE;
            }

            break;


        case WIFI_INITIAL_STATE:
            const char *user_code;
            user_code = request_user_and_device_code();

            char *my_user_code;
            my_user_code= strdup(user_code);

            swSer.write(0x2d);
            swSer.write(0x5a);
            swSer.write(CMD_SHOW_USERCODE_CALENDAR);
            WriteToCalendar(my_user_code);

            Serial.println("*********************");
            Serial.print("USER-CODE:  ");
            Serial.println(user_code);
            Serial.println("*********************");

            DPL("Wait Calendar - UserCode  DONE");
            if (WaitForCalendarStatus() == CALENDAR_STATUS_DONE) {
                break;
            }

            global_status = WIFI_AWAIT_CHALLENGE;
            break;

        case WIFI_AWAIT_CHALLENGE:
            Serial.print("Await challenge for Device:");
            Serial.println(rtcOAuth.device_code);
            global_status = poll_authorization_server();
            break;

        case WIFI_CHECK_ACCESS_TOKEN:
            if (global_access_token[0] == 0) {
                Serial.print("Refresh Access Token");
                global_status = request_access_token();
                break;
            }
            global_status = CAL_PAINT_UPDATE;
            break;


        case CAL_PAINT_UPDATE:

            //xxx REMOVE ME !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1
//            global_status = ESP_START_SLEEP;
//            break;

            if (global_access_token[0] == 0) {
                global_status = request_access_token();
                break;
            }


            // Send Commando to calendar: Request calendar infos and request form Google

            swSer.write(0x2d);
            swSer.write(0x5a);
            swSer.write(CMD_READ_CALENDAR);


            strftime(str_time, sizeof(str_time), "%Y %m %d %H:%M:%S", &global_time);
            DP("******* Request Calendar Update with time-stamp:");
            DPL(str_time);
            WriteToCalendar(str_time);

            while (true) {
                DPL("Wait Calendar Ready");
                while (WaitForCalendarStatus() != CALENDAR_READY) {}
                request = ReadSWSer();

                DP("***************************   Request received: ");
                DPL(request);

                if (calendarGetRequest(host, request)) {
                    DPL("******* SUCCESS*******");
                } else {
                    DPL("******* ERROR*******");
                }

                DPL("Wait Calendar DONE");
                if (WaitForCalendarStatus() == CALENDAR_STATUS_DONE) {
                    break;
                }

            }

            DPL("***************************   Requests done ********************");

            global_status = CAL_PAINT_DONE;

            break;

        case CAL_PAINT_DONE:
            delay(500);
            swSer.write(CMD_SHUTDOWN_CALENDAR);
            delay(500);

            digitalWrite(PIN_POWER_CAL, LOW);
            global_status = ESP_START_SLEEP;
            break;

        case ESP_START_SLEEP:

            DPL("*** PREPARE DEEP SLEEP ***");

            // Calculate needed sleep time

            uint16_t sleep_min;
            /*uint16_t overrun_hour=0;

            // Calculate next wakeup time
            if (WAKE_UP_MIN - global_time.tm_min >= 0) {

            } else {
                sleep_min = global_time.tm_min - WAKE_UP_MIN;
            }
*/
            sleep_min = (WAKE_UP_MIN - global_time.tm_min);

            DP("Current Time(MM):");DPL(global_time.tm_min);
            DP("Wakeup-Min:");DPL(WAKE_UP_MIN);
            DP("Min to sleep:");DPL(sleep_min);


            uint8_t sleep_hour;
            // Calculate next wakeup time
            if (WAKE_UP_HOUR - global_time.tm_hour >= 0) {
                sleep_hour = WAKE_UP_HOUR - global_time.tm_hour;
            } else {
                sleep_hour = global_time.tm_hour - WAKE_UP_HOUR+24;
            }

            DP("Current Time(HH):");DPL(global_time.tm_hour);
            DP("Wakeup-Hour:");DPL(WAKE_UP_HOUR);
            DP("Hours to sleep: ");DPL(sleep_hour);




            sleep_min = sleep_min + 60 * sleep_hour;
            DP("Total Min to sleep:");DPL(sleep_min);

            uint16_t my_sleep_max_min;

            my_sleep_max_min= ESP.deepSleepMax()/60000000;
            DP("ESP Max-Sleeptime in minutes:");DPL(my_sleep_max_min);


            RTC_WakeUpRead();
            rtcWakeUp.wakeup_count = (sleep_min / my_sleep_max_min);
            rtcWakeUp.remaining_sleep_min = sleep_min % my_sleep_max_min;
            RTC_WakeUpWrite();

            DP("Max-Cycles to sleep:");DPL(rtcWakeUp.wakeup_count); //count==1, take the minutes
            DP("Minutes to sleep:");DPL(rtcWakeUp.remaining_sleep_min);

            if (rtcWakeUp.wakeup_count == 0) {
                DP("DeepSleep for min:");DPL(rtcWakeUp.remaining_sleep_min);
                DPL("** GOOD NIGHT!**");delay(500);
                ESP.deepSleep(rtcWakeUp.remaining_sleep_min * 60000);
            } else {
                DPL("** GOOD NIGHT!**");delay(500);
                ESP.deepSleep(ESP.deepSleepMax());
            }

//            ESP.deepSleep(20000000);


            DPL("*** Press key to continue, 'x' to reset authentication");
            while (true) {
                yield();
                int c = SerialKeyWait();
                Serial.print("Typed:");
                Serial.println(c);
                if (c == 120) {
                    global_status = WIFI_INITIAL_STATE;
                } else {
                    global_status = CAL_WAIT_READY;
                }
                break;

            }
            break;

        case ESP_SEND_ERROR_MSG:

            strftime(str_time, sizeof(str_time), "%H:%M:%S", &global_time);


            my_error_msg= (char *)malloc(strlen(str_time) + strlen(error_msg) + 1);

            strcpy(my_error_msg, str_time);
            strcat(my_error_msg, error_msg);

            DP("ERROR MESSAGE to Calender:");DPL(my_error_msg);

            swSer.write(0x2d);
            swSer.write(0x5a);
            swSer.write(CMD_SHOW_ERROR_MSG);
            delay(500);
            WriteToCalendar(my_error_msg);
            while (WaitForCalendarStatus() != CALENDAR_READY) {}

            global_status=CAL_PAINT_DONE;

            break;

        default:
            Serial.println("ERROR");
            break;
    }

    yield();

}


