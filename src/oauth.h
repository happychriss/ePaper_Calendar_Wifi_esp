//
// Created by development on 05.01.19.
//

#ifndef ESP8266_WIFI_OAUTH_H
#define ESP8266_WIFI_OAUTH_H


#include "softser_old.h"

void RTC_OAuthWrite();

bool RTC_OAuthRead();
void RTC_WakeUpWrite();
bool RTC_WakeUpRead();

extern uint8_t global_status ;

extern struct rtcDataOauthStruct rtcOAuth;
extern struct rtcDataWakeupStruct rtcWakeUp;

extern char * global_access_token;
extern SoftwareSerial swSer;
void ReadSWSer(char *data);
uint8_t request_access_token();

void SetupTimeSNTP(tm *timeinfo);
bool SetupMyWifi(const char *ssid, const char *password);
int SerialKeyWait();

extern const char * request_user_and_device_code();
uint8_t poll_authorization_server();
bool CheckCertifcates();

bool calendarGetRequest(char *request);
bool CheckRTC();

#endif //ESP8266_WIFI_OAUTH_H
