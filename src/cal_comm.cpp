//
// Created by development on 13.04.19.
//

#include <ESP8266WiFi.h>
#include "string"
#include "oauth.h"
#include "helper.h"
#include <SoftwareSerial.h>
#include "cal_comm.h"

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

uint8_t WaitForCalendarStatus() {

    DPL("Wait Status:");

    int c1, in;

        while (true) {
            in = swSer.available();
            delay(50);
            if (in > 0) break;
        }
        c1 = swSer.read();

      DP("Got Status:");DPL(c1);

    return c1;


}


