//
// Created by development on 13.04.19.
//

#ifndef ESP8266_WIFI_CAL_COMM_H
#define ESP8266_WIFI_CAL_COMM_H
void ReadFromCalendar(char *data);
void WriteToCalendar(char *data);
uint8_t WaitForCalendarStatus();
#endif //ESP8266_WIFI_CAL_COMM_H
