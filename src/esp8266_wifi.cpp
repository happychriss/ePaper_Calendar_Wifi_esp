
#include <ESP8266WiFi.h>
//#include <WiFiClientSecureBearSSL.h>
#include <WiFiClientSecureAxTLS.h> // force use of AxTLS (BearSSL is now default)
#include "string"
#include "oauth.h"
#include "helper.h"
#include <SoftwareSerial.h>
#include "EEPROM.h"

//receivePin, transmitPin,
SoftwareSerial swSer(12, 13, false, 256); //STM32: Weiß muss an RX, Grun an TX

const char *ssid = "XXXXX";
const char *password = "XXXXX;
const char *host = "www.googleapis.com";


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored  "-Wdeprecated-declarations"


WiFiClientSecure client;

#pragma GCC diagnostic pop


rtcDataStruct rtcData;
char global_access_token[150] = "ya29.GluPBv80YVsTmc5uCIDt5AoEDbdoP-srm1zk7zzfF-1qRCmOHwcTlXsIgGGKJmNufn5XaRSAdh4B-T4gkizphT20SFW54IlyMZD7wepBC-2OW9W1elL8Z6swZTcO\0";
// char global_access_token[150];

// Set global variable attributes.
uint8_t global_status;


void setup() {
    Serial.begin(9600);
    swSer.begin(9600);
    int c;
    bool b_reset_authorization = false;
    DPL("Key to start, 'x' to reset authentication");
    c = SerialKeyWait();
    DP("Typed:");
    DPL(c);
    if (c == 120) { //x
        b_reset_authorization = true;
    }


    SetupMyWifi(ssid, password);
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

}


void loop() {


    DP("***** State:");
    DPD(global_status);
    DPL("****");

    String request;

    switch (global_status) {

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
            global_status = CAL_WAIT_READY;
            break;

        case CAL_WAIT_READY:
            Serial.print("Waiting for calender to be ready...");

            int c;

            while (true) {
                int in = swSer.available();
                delay(50);
                if (in > 0) break;
            }

            c = swSer.read();
            Serial.print("Read:");Serial.println(c);

            if (c == 2) {
                global_status=CAL_PAINT_UPDATE;
                    break;
                }

            break;



        case CAL_PAINT_UPDATE:



            if (global_access_token[0] == 0) {
                global_status = request_access_token();
                break;
            }

            DPL("******* Request Calendar Update  **********");

            swSer.write(0x2d);
            swSer.write(0x5a);
            #define CMD_READ_CALENDAR 1
            swSer.write(CMD_READ_CALENDAR);
            swSer.flush();
            delay(100);

            request = ReadSWSer();

            DP("Request received: "); DPL(request);

            if (calendarGetRequest(host, request)) {
                DPL("******* SUCCESS*******");
            } else {
                DPL("******* ERROR*******");
            }

            global_status = CAL_PAINT_DONE;

            break;

        case CAL_PAINT_DONE:
            DPL("*** Press key to continue, 'x' to reset authentication");
            while (true) {
                yield();
                int c = SerialKeyWait();
                Serial.print("Typed:");Serial.println(c);
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


