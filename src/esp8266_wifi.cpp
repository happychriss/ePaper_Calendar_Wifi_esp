#define WAKE_UP_HOUR 11
#define WAKE_UP_MIN 27

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
#include <support.h>



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

// #define WAIT_SERIAL

#ifdef WAIT_SERIAL
    Serial.begin(115200);
    int c;
    DPL("max deep sleep: " + uint64ToString(ESP.deepSleepMax()));
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



    CP("************** State:");CPD(global_status);CPL(" ****************");

    String request;

    switch (global_status) {

        case WAKE_UP_FROM_SLEEP:

            if (RTC_WakeUpRead()) {

                if (rtcWakeUp.wakeup_count > 1) {
                    CP("DeepSleep for Max-Time with count:");CPL(rtcWakeUp.wakeup_count);
                    rtcWakeUp.wakeup_count = rtcWakeUp.wakeup_count - 1;
                    RTC_WakeUpWrite();
                    ESP.deepSleep(ESP.deepSleepMax()/10);
                }

                if (rtcWakeUp.wakeup_count == 1) {
                    rtcWakeUp.wakeup_count = rtcWakeUp.wakeup_count - 1;
                    RTC_WakeUpWrite();
                    CP("DeepSleep for min:");CPL(rtcWakeUp.remaining_sleep_min);
                    ESP.deepSleep((rtcWakeUp.remaining_sleep_min * 60000000)/10);
                }

                if (rtcWakeUp.wakeup_count == 0) {
                    global_status = CAL_WAIT_READY;
                    CPL("DeepSleep: DONE - Start Calendar Flow");
                }
            } else {
                global_status = CAL_WAIT_READY;
                CPL("DeepSleep: No Valid RTC found - Start Calendar Flow");
            }


            break;

        case CAL_WAIT_READY:
            digitalWrite(PIN_POWER_CAL, HIGH);
            delay(1000);
            CPL("Waiting for calender to be ready...");

            while (WaitForCalendarStatus() != CALENDAR_READY) {}

            global_status = CAL_WIFI_GET_CONFIG;
            break;


        case CAL_WIFI_GET_CONFIG:

            CPL("Getting Wifi Config from Calendar..");

            ReadFromCalendar(my_ssid);
            CP("SSID:");
            CPL(my_ssid);

            ReadFromCalendar(my_pwd);
            CP("PWD: ");
            CPL(my_pwd);

            global_status = WIFI_INIT;

            break;

        case WIFI_INIT:
            CPL("Init Wifi");

            if (SetupMyWifi(my_ssid, my_pwd)) {
             error_msg=strdup(" - WIFI Connection ERROR!");
             global_status=ESP_SEND_ERROR_MSG;
             break;
            }

            SetupTimeSNTP(&global_time);


            if (RTC_OAuthRead()) {
                CPL("** OAUTH RTC Status");
                CP("Status: ");
                CPL(rtcOAuth.status);
                CP("Device-Code: ");
                CPL(rtcOAuth.device_code);
                CP("Refresh-Token: ");
                CPL(rtcOAuth.refresh_token);

                global_access_token[0] = 0;

                if (b_reset_authorization) {
                    CPL("*Reset Google User access - set initial state*");
                    global_status = WIFI_INITIAL_STATE;
                } else {
                    global_status = rtcOAuth.status;
                }

            } else {
                CPL("** Invalid RTC Status - Reauth");;
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

            CPL("*********************");
            CP("USER-CODE:  ");
            CPL(user_code);
            CP("*********************");

            CPL("Wait Calendar - UserCode  DONE");
            if (WaitForCalendarStatus() == CALENDAR_STATUS_DONE) {
                break;
            }

            global_status = WIFI_AWAIT_CHALLENGE;
            break;

        case WIFI_AWAIT_CHALLENGE:
            CP("Await challenge for Device:");
            CPL(rtcOAuth.device_code);
            global_status = poll_authorization_server();
            break;

        case WIFI_CHECK_ACCESS_TOKEN:
//xxx
//            global_status = ESP_START_SLEEP;
//            break;

            if (global_access_token[0] == 0) {
                CPL("Refresh Access Token");
                global_status = request_access_token();
                break;
            }
            global_status = CAL_PAINT_UPDATE;
            break;


        case CAL_PAINT_UPDATE:

            if (global_access_token[0] == 0) {
                global_status = request_access_token();
                break;
            }


            // Send Commando to calendar: Request calendar infos and request form Google
            swSer.write(0x2d);
            swSer.write(0x5a);
            swSer.write(CMD_READ_CALENDAR);


            strftime(str_time, sizeof(str_time), "%Y %m %d %H:%M:%S", &global_time);
            CP("******* Request Calendar Update with time-stamp:");
            CPL(str_time);
            WriteToCalendar(str_time);

            while (true) {
                CPL("Wait Calendar Ready");
                while (WaitForCalendarStatus() != CALENDAR_READY) {}
                request = ReadSWSer();

                DP("***************************   Request received: ");
                DPL(request);

                if (calendarGetRequest(host, request)) {
                    CPL("******* SUCCESS: Calendar Update done *******");
                } else {
                    CPL("******* ERROR: Calendar Update error*******");
                }

                CPL("Wait Calendar DONE");
                if (WaitForCalendarStatus() == CALENDAR_STATUS_DONE) {
                    break;
                }

            }

            CPL("***************************   Requests done ********************");

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

            CalculateSleepUntil(WAKE_UP_HOUR, WAKE_UP_MIN);

            CPL("*** PREPARE DEEP SLEEP ***");

            if (rtcWakeUp.wakeup_count == 0) {
                CP("DeepSleep for min:");CPL(rtcWakeUp.remaining_sleep_min);
                CPL("** GOOD NIGHT!**");delay(500);
                ESP.deepSleep((rtcWakeUp.remaining_sleep_min * 60000000)/10);
            } else {
                CPL("** GOOD NIGHT!**");delay(500);
                ESP.deepSleep(ESP.deepSleepMax()/10);
            }
            // here we will not arrive :-)

#ifdef MYDEBUG

            CPL("*** Press key to continue, 'x' to reset authentication");
            while (true) {
                yield();
                int c = SerialKeyWait();
                CP("Typed:");
                CPL(c);
                if (c == 120) {
                    global_status = WIFI_INITIAL_STATE;
                } else {
                    global_status = CAL_WAIT_READY;
                }
                break;

            }
#endif
            break;

        case ESP_SEND_ERROR_MSG:

            strftime(str_time, sizeof(str_time), "%H:%M:%S", &global_time);


            my_error_msg= (char *)malloc(strlen(str_time) + strlen(error_msg) + 1);

            strcpy(my_error_msg, str_time);
            strcat(my_error_msg, error_msg);

            CP("ERROR MESSAGE to Calender:");CPL(my_error_msg);

            swSer.write(0x2d);
            swSer.write(0x5a);
            swSer.write(CMD_SHOW_ERROR_MSG);
            delay(500);
            WriteToCalendar(my_error_msg);
            while (WaitForCalendarStatus() != CALENDAR_READY) {}

            global_status=CAL_PAINT_DONE;

            break;

        default:
            CPL("ERROR");
            break;
    }

    yield();

}


