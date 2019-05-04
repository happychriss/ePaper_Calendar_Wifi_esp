//
// Created by development on 03.05.19.
//

#ifndef ESP8266_WIFI_SUPPORT_H
#define ESP8266_WIFI_SUPPORT_H

#include <time.h>
#include <helper.h>
#include <stdint.h>
void CalculateSleepUntil(uint8_t wake_up_hour, uint8_t wake_up_min);
void RTC_OAuthWrite();

bool RTC_OAuthRead();
void RTC_WakeUpWrite();
bool RTC_WakeUpRead();

extern uint8_t global_status ;

extern struct rtcDataOauthStruct rtcOAuth;
extern struct rtcDataWakeupStruct rtcWakeUp;

extern tm global_time;
#endif //ESP8266_WIFI_SUPPORT_H
