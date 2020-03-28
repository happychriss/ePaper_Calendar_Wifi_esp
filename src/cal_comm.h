//
// Created by development on 13.04.19.
//

#ifndef ESP8266_WIFI_CAL_COMM_H
#define ESP8266_WIFI_CAL_COMM_H
void ReadFromCalendar(char *data);
void WriteToCalendar(char *data);
int WaitForCalendarStatus();
String uint64ToString(uint64_t input);
bool WriteCommandToCalendar(uint8 command);
#endif //ESP8266_WIFI_CAL_COMM_H
