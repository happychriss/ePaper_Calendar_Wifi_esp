//
// Created by development on 13.04.19.
//

#include <ESP8266WiFi.h>
#include "string"
#include "oauth.h"
#include "main_esp8266_wifi.h"
#include "cal_comm.h"
#include "main_esp8266_wifi.h"




void ReadFromCalendar(char *data) {
    int count = 0;

    while (true) {
        int in = swSer.available();
        delay(50);
        if (in > 0) break;
    }

    while (swSer.available()) {
        delay(2);  //delay to allow byte to arrive in input buffer
        char d = swSer.read();
        data[count] = d;
        count++;
    }
    data[count] = 0;
}

void WriteToCalendar(char *data) {
    int count = 0;
    while (true) {
        swSer.write(data[count]);
        if (data[count] == 0) break;
        count++;
    }
    swSer.flush();
    delay(100);

}

int WaitForCalendarStatus() {

    DPL("Wait Status:");
    uint16 waiter=0;

    int c1, in;

        while (waiter<100) {
            in = swSer.available();
            delay(50);
            if (in > 0) break;
            waiter++;
        }
        c1 = swSer.read();

      DP("Got Status:");DPL(c1);

    return c1;


}

bool WriteCommandToCalendar(uint8 command) {

    delay(500);
    DP("WriteCommand to calender:");DPL(command);
    swSer.write(0x2d);
    swSer.write(0x5a);
    swSer.write(command);
    return true;
 }

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