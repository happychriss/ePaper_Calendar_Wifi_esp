//
// Created by development on 05.01.19.
//

#ifndef ESP8266_WIFI_OAUTH_H
#define ESP8266_WIFI_OAUTH_H


#include <SoftwareSerial.h>

void EEPromMemoryRead(uint32_t offset, uint32_t *data, size_t size);
void EEPromMemoryWrite(uint32_t offset, uint32_t *data, size_t size);

extern uint8_t global_status ;

extern struct rtcDataStruct rtcData;
extern char global_access_token[150];
extern SoftwareSerial swSer;
extern const char *host;
String ReadSWSer();
uint8_t request_access_token();

void SetupTimeSNTP(tm *timeinfo);
void SetupMyWifi(const char *ssid, const char *password);
int SerialKeyWait();

extern const char * request_user_and_device_code();
uint8_t poll_authorization_server();
bool calendarGetRequest(const char *server, String request);
bool CheckRTC();

#endif //ESP8266_WIFI_OAUTH_H
