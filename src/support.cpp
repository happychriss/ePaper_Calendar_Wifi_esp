//
// Created by development on 03.05.19.
//

#include <Esp.h>
#include "support.h"
#include "oauth.h"
#include "main_esp8266_wifi.h"
#include "EEPROM.h"




// Calculate needed sleep time

void LED_Blink(uint8_t no) {
    while (no!=0) {
        no--;
        digitalWrite(PIN_LED, HIGH);
        delay(200);
        digitalWrite(PIN_LED, LOW);
        delay(200);
    }
    digitalWrite(PIN_LED, HIGH);
    delay(400);
    digitalWrite(PIN_LED, LOW);
}


void MyDeepSleep(uint16_t min, RFMode mode) {
    uint64_t us2min = US2MIN;

    DP("MyDeepSleep: ");DPL(min/WARP_FACTOR);

    uint64_t sleep_us=min * us2min;
    sleep_us=sleep_us/WARP_FACTOR;
    ESP.deepSleep(sleep_us,mode);
}


void CalculateSleepUntil(uint8_t wake_up_hour, uint8_t wake_up_min) {

    int sleep_hour;
    int sleep_min;

    DP("Current Time(HH:MM): ");
    DP(global_time.tm_hour);
    DP(":");
    DPL(global_time.tm_min);
    DP("Wakeup Time(HH:MM): ");
    DP(wake_up_hour);
    DP(":");
    DPL(wake_up_min);

    sleep_min = (wake_up_min - global_time.tm_min);

// Calculate next wakeup time
    if (wake_up_hour - global_time.tm_hour >= 0) {
        sleep_hour = wake_up_hour - global_time.tm_hour;
    } else {
        sleep_hour = wake_up_hour - global_time.tm_hour + 24;
    }

    DP("Hours to sleep: ");
    DPL(sleep_hour);
    DP("Min to sleep: ");
    DPL(sleep_min);


    sleep_min = sleep_min + 60 * sleep_hour;
    if (sleep_min <= 0) {
        sleep_min = sleep_min + 24 * 60;
    }

    CP("Total Min to sleep:");CPL(sleep_min);

    uint16_t my_sleep_max_min;

    my_sleep_max_min = MAX_SLEEP_MIN;
    DP("ESP Max-Sleeptime per cycle in minutes:");
    DPL(my_sleep_max_min);

    RTC_WakeUpRead();
    rtcWakeUp.wakeup_count = (sleep_min / my_sleep_max_min);

    rtcWakeUp.remaining_sleep_min = sleep_min % my_sleep_max_min;
    if ( rtcWakeUp.remaining_sleep_min==0) {
        rtcWakeUp.remaining_sleep_min=1;
    }

    RTC_WakeUpWrite();

    CP("Max-Cycles to sleep:"); CPL(rtcWakeUp.wakeup_count); //count==1, take the minutes
    CP("Minutes to sleep:"); CPL(rtcWakeUp.remaining_sleep_min);


}



void ReadSWSer(char *data) {

    uint16_t bytes_read = 0;
    int c;

    while (true) {

        c = swSer.read();
        delay(20);
        if ((c == 0) || (c == -1) ) {
            break;
        }

        data[bytes_read++] = (char) c;

    }

    data[bytes_read]=0;

    DP("Read Buffer Size:");DPL(bytes_read);

    DPL(data);

}


uint32_t calculateCRC32(const uint8_t *data, size_t length) {
    uint32_t crc = 0xffffffff;
    while (length--) {
        uint8_t c = *data++;
        for (uint32_t i = 0x80; i > 0; i >>= 1) {
            bool bit = crc & 0x80000000;
            if (c & i) {
                bit = !bit;
            }

            crc <<= 1;
            if (bit) {
                crc ^= 0x04c11db7;
            }
        }
    }

    return crc;
}


void RTC_OAuthWrite() {
    DPL("RTC_OAuthWrite-Start");
    rtcOAuth.crc32 = calculateCRC32(((uint8_t *) &rtcOAuth) + 4, sizeof(rtcOAuth) - 4);

    uint addr = 0;
    EEPROM.put(addr, rtcOAuth);
    EEPROM.commit();
    DPL("RTC_OAuthWrite-End");
}


void RTC_WakeUpWrite() {
    rtcWakeUp.crc32 = calculateCRC32(((uint8_t *) &rtcWakeUp) + 4, sizeof(rtcWakeUp) - 4);

    uint addr = sizeof(rtcOAuth)+1;
    EEPROM.put(addr, rtcWakeUp);
    EEPROM.commit();
}


bool RTC_OAuthRead() {

    bool rtcValid = false;
    EEPROM.get(0, rtcOAuth);

    // Calculate the CRC of what we just read from RTC memory, but skip the first 4 bytes as that's the checksum itself.
    uint32_t crc = calculateCRC32(((uint8_t *) &rtcOAuth) + 4, sizeof(rtcOAuth) - 4);
    if (crc == rtcOAuth.crc32) {
        rtcValid = true;
        DPL("RTC: OAuth-Read - Valid RTC");
        DPL(crc);

    } else {
        memset(&rtcOAuth, 0, sizeof(rtcDataOauthStruct));
        DPL("RTC: OAuth-Read-  InValid RTC");
    }

    return rtcValid;


}

/*
bool CheckRTC() {

    bool rtcValid = false;

    EEPROM.get(0, rtcData);
// Calculate the CRC of what we just read from RTC memory, but skip the first 4 bytes as that's the checksum itself.
    uint32_t crc = calculateCRC32(((uint8_t *) &rtcData) + 4, sizeof(rtcData) - 4);
    if (crc == rtcData.crc32) {
        rtcValid = true;
        DPL("Valid RTC");
        DPL(crc);

    } else {
        DPL("InValid RTC");
    }

    return rtcValid;
}
*/


bool RTC_WakeUpRead() {
    uint addr = sizeof(rtcDataOauthStruct)+1; //just take next place in EEPROM

    EEPROM.get(addr, rtcWakeUp);
    bool rtcValid = false;

    // Calculate the CRC of what we just read from RTC memory, but skip the first 4 bytes as that's the checksum itself.
    uint32_t crc = calculateCRC32(((uint8_t *) &rtcWakeUp) + 4, sizeof(rtcWakeUp) - 4);

    if (crc == rtcWakeUp.crc32) {
        rtcValid = true;
        DPL("RTC: Wakeup-Read - Valid RTC");

    } else {
        memset(&rtcWakeUp, 0, sizeof(rtcWakeUp));
        DPL("RTC: Wakeup-Read - InValid RTC");
    }

    return rtcValid;

}


/*bool CheckRTC() {

    bool rtcValid = false;

    EEPROM.get(0, rtcData);
// Calculate the CRC of what we just read from RTC memory, but skip the first 4 bytes as that's the checksum itself.
    uint32_t crc = calculateCRC32(((uint8_t *) &rtcData) + 4, sizeof(rtcData) - 4);
    if (crc == rtcData.crc32) {
        rtcValid = true;
        DPL("Valid RTC");
        DPL(crc);

    } else {
        memset(&rtcData, 0, sizeof(struct rtcDataOauthStruct));
        DPL("InValid RTC");
    }

    return rtcValid;
}*/


int SerialKeyWait() {// Wait for Key
//    Serial.setDebugOutput(true);

    Serial.println("Hit a key to start...");
    Serial.flush();

    while (true) {
        int inbyte = Serial.available();
        delay(500);
        if (inbyte > 0) break;
    }

    return Serial.read();


}

void ErrorToDisplay (const char *my_error_msg) {
    char str_time[22];

    strftime(str_time, sizeof(str_time), "%d%m%y%H%M", &global_time);

    ssize_t bufsz = snprintf(NULL, 0,
                             "%s:%s", str_time, my_error_msg);

    global_error_msg = (char *) malloc(bufsz + 1);
    memset(global_error_msg,0,bufsz+1);

    sprintf(global_error_msg,
            "%s:%s", str_time, my_error_msg);

    CP("ERROR MESSAGE to Calender:");
    CPL(my_error_msg);
}