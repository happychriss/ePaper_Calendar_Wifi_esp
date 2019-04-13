
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

//receivePin, transmitPin,
SoftwareSerial swSer(12, 13, false, 256); //STM32: Weiß muss an RX, Grun an TX

const char *host = "www.googleapis.com";

char my_ssid[50] = {0};
char my_pwd[50] = {0};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored  "-Wdeprecated-declarations"


WiFiClientSecure client;
tm global_time;


#pragma GCC diagnostic pop


rtcDataStruct rtcData;
char global_access_token[150] = "ya29.GluPBv80YVsTmc5uCIDt5AoEDbdoP-srm1zk7zzfF-1qRCmOHwcTlXsIgGGKJmNufn5XaRSAdh4B-T4gkizphT20SFW54IlyMZD7wepBC-2OW9W1elL8Z6swZTcO\0";
// char global_access_token[150];

// Set global variable attributes.
uint8_t global_status;
bool b_reset_authorization = false;


// Commandos to Calendar
#define CMD_READ_CALENDAR 1

// If the calendar wants to get more data
#define CALENDAR_READY 1
#define CALANDAR_STATUS_MORE  2
#define CALENDAR_STATUS_DONE  3


void setup() {
    Serial.begin(115200);
    swSer.begin(9600);
    int c;

    DPL("Key to start, 'x' to reset authentication");
    c = SerialKeyWait();
    DP("Typed:");
    DPL(c);
    if (c == 120) { //x
        b_reset_authorization = true;
    }


    global_status=CAL_WAIT_READY;
}


void loop() {

    int count = 0;

    DP("***** State:");
    DPD(global_status);
    DPL("****");

    String request;

    switch (global_status) {

        case CAL_WAIT_READY:
            Serial.print("Waiting for calender to be ready...");

            while(WaitForCalendarStatus()!=CALENDAR_READY){}

            global_status = CAL_WIFI_GET_CONFIG;
            break;


        case CAL_WIFI_GET_CONFIG:

            Serial.println("Getting Wifi Config from Calendar..");

            ReadFromCalendar(my_ssid);
            DP("SSID:");DPL(my_ssid);

            ReadFromCalendar(my_pwd);
            DP("PWD: ");DPL(my_pwd);

            global_status = WIFI_INIT;

            break;

        case WIFI_INIT:
            Serial.println("Init Wifi");

            SetupMyWifi(my_ssid, my_pwd);
            SetupTimeSNTP(&global_time);

            EEPROM.begin(250);
            EEPROM.get(0, rtcData);

            if (CheckRTC()) {
                DP("Status: ");
                DPL(rtcData.status);
                DP("Device-Code: ");
                DPL(rtcData.device_code);
                DP("Refresh-Token: ");
                DPL(rtcData.refresh_token);
                global_access_token[0] = 0;

                if (b_reset_authorization) {
                    Serial.println("*Reset Google User access - set initial state*");
                    global_status = WIFI_INITIAL_STATE;
                } else {
                    global_status = rtcData.status;
                }

            } else {
                DPL("** Invalid RTC Status - Reauth");;
                global_status = WIFI_INITIAL_STATE;
            }
            break;




        case WIFI_INITIAL_STATE:
            const char *user_code;
            user_code = request_user_and_device_code();
            Serial.println("*********************");
            Serial.print("USER-CODE: ");
            Serial.println(user_code);
            Serial.println("*********************");

            global_status = WIFI_AWAIT_CHALLENGE;
            break;

        case WIFI_AWAIT_CHALLENGE:
            Serial.print("Await challenge for Device:");
            Serial.println(rtcData.device_code);
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


            if (global_access_token[0] == 0) {
                global_status = request_access_token();
                break;
            }


            swSer.write(0x2d);
            swSer.write(0x5a);


            // Send Commando to calendar: Request calendar infos and request form Google

            swSer.write(CMD_READ_CALENDAR);
            char time[22];
            strftime(time, sizeof(time), "%Y %m %d %H:%M:%S", &global_time);
            DP("******* Request Calendar Update with time-stamp:"); DPL(time);
            WriteToCalendar(time);



            while(true) {
                DPL("Wait Calendar Ready");
                while(WaitForCalendarStatus()!=CALENDAR_READY){}
                request = ReadSWSer();

                DP("***************************   Request received: ");
                DPL(request);

                if (calendarGetRequest(host, request)) {
                    DPL("******* SUCCESS*******");
                } else {
                    DPL("******* ERROR*******");
                }

                DPL("Wait Calendar DONE");
                if (WaitForCalendarStatus()==CALENDAR_STATUS_DONE) {
                    break;
                }

            }

            DP("***************************   Requests done");

            global_status = CAL_PAINT_DONE;

            break;

        case CAL_PAINT_DONE:
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

        default:
            Serial.println("ERROR");
            break;
    }

    yield();

}


