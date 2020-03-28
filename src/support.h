//
// Created by development on 03.05.19.
//

#ifndef ESP8266_WIFI_SUPPORT_H
#define ESP8266_WIFI_SUPPORT_H

#include <time.h>
#include <main_esp8266_wifi.h>
#include <stdint.h>

void CalculateSleepUntil(uint8_t wake_up_hour, uint8_t wake_up_min);
void RTC_OAuthWrite();

bool RTC_OAuthRead();
void RTC_WakeUpWrite();
bool RTC_WakeUpRead();

void LED_Blink(uint8_t no);

void MyDeepSleep(uint16_t min, RFMode mode);

extern uint8_t global_status ;

extern struct rtcDataOauthStruct rtcOAuth;
extern struct rtcDataWakeupStruct rtcWakeUp;
extern char * global_error_msg;
void ErrorToDisplay (char * my_error_msg);
extern tm global_time;
#endif //ESP8266_WIFI_SUPPORT_H
