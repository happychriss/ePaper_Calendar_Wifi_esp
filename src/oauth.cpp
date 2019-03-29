//
// Created by development on 05.01.19.
//


#include <WString.h>
#include "HardwareSerial.cpp"
#include <ESP8266WiFi.h>
#include "cacert.h"
#include <time.h>
#include "ArduinoJson.h"
#include "helper.h"
#include "oauth.h"
#include "EEPROM.h"
// #include <CertStoreBearSSL.h>

// OAUTH2 Client credentials
const String client_id = "88058113591-7ek2km1rt9gsjhlpb9fuckhl8kpnllce.apps.googleusercontent.com";
const String scope = "https://www.googleapis.com/auth/calendar";
const String auth_uri = "https://accounts.google.com/o/oauth2/auth";
const String code_uri = "https://accounts.google.com/o/oauth2/device/code";
const String info_uri = "/oauth2/v3/tokeninfo";
const String token_uri = "/oauth2/v4/token";


const int httpsPort = 443;


String ReadSWSer() {

    char buffer[200] = {};
    uint8_t bytes_read = 0;
    int c;

    while (true) {

        c = swSer.read();
        delay(50);
        if (((c == 0) || (c == -1))) {
            break;
        }

        buffer[bytes_read++] = (char) c;
    }

    buffer[bytes_read] = 0;
    String result((char *) buffer);
    return result;
}


void EEPromMemoryRead(uint32_t offset, uint32_t *data, size_t size) {
    uint addr = offset;
    EEPROM.get(addr, data);

}

void EEPromMemoryWrite(uint32_t offset, uint32_t *data, size_t size) {
    uint addr = offset;
    EEPROM.put(addr, data);
    EEPROM.commit();
};


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


void SetupMyWifi(const char *ssid, const char *password) {
    Serial.println();
    Serial.print("connecting to ");
    Serial.println(ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    // Synchronize time useing SNTP. This is necessary to verify that
// the TLS certificates offered by the server are currently valid.
    Serial.print("Setting time using SNTP");
    configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    time_t now = time(nullptr);
    while (now < 8 * 3600 * 2) {
        delay(500);
        Serial.print(".");
        now = time(nullptr);
    }
    Serial.println("");
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    Serial.print("Current time: ");
    Serial.print(asctime(&timeinfo));
}

// Root certificate used by google at  https://pki.goog/
// Only works with RSA 2048, SHA-1  https://pki.goog/gsr2/GSR2.crt, expires   15.12.2021
// error 514 - no valid certificate chain shown, to see error run with debug option

void SetCertificates(WiFiClientSecure client) {
    // Load root certificate in DER format into WiFiClientSecure object

    bool res;
//    int numCerts = certStore.initCertStore(&certs_idx, &certs_ar);
    res = client.setCACert_P(caCert2, caCertLen2);
    if (!res) {
        DPL("Failed to load root CA certificate 2!");
        while (true) { yield(); }
    }

    res = client.setCACert_P(caCert3, caCertLen3);
    if (!res) {
        DPL("Failed to load root CA certificate 3!");
        while (true) { yield(); }
    }

    res = client.setCACert_P(caCert1, caCertLen1);
    if (!res) {
        DPL("Failed to load root CA certificate 1!");
        while (true) { yield(); }
    }

    DPL("Set Certificates");
}


bool calendarGetRequest(const char *server, String request) {


    DP("calendarGetRequest:");
    DPL(request);

    bool result = false;

    String reqHeader = "";
    reqHeader += ("GET " + request + "access_token=" + global_access_token + " HTTP/1.1\r\n");
    reqHeader += ("Host: " + String(host) + ":" + String(httpsPort) + "\r\n");
    reqHeader += ("Connection: close\r\n");
    reqHeader += ("\r\n\r\n");

    // Use WiFiClientSecure class to create TLS connection
    WiFiClientSecure client;
    DP("Connecting to: ");
    DPL(server);

    if (!client.connect(server, httpsPort)) {
        DPL("connection failed");
        return result;
    }

    SetCertificates(client);

    if (client.verifyCertChain(server)) {

        DPL("certificate matches");
        DP("get: ");
        DPL(reqHeader);
        client.print(reqHeader);
        DPL("request sent");
        DPL("Receiving response");

        if (client.find("HTTP/1.1 ")) {
            String status_code = client.readStringUntil('\r');
            DP("Status code: ");
            DPL(status_code);
            if (status_code != "200 OK") {
                DPL("There was an error");
                return false;
            }
        }
        if (client.find("\r\n\r\n")) {
            DPL("Data:");
        }

#define READ_LENGTH 1

        char buffer[1];
        size_t b;

        while (true) {
            b = client.readBytesUntil('\r', buffer, (size_t) READ_LENGTH);
            DP(buffer[0]);
            if (b == READ_LENGTH) {
                swSer.write((uint8_t) buffer[0]);
            } else {
                uint8_t v = 0;
                swSer.write(v);
                break;
            }
        }

        result = true;

        DPL("closing connection");

        return result;
    } else {
        DPL("certificate doesn't match");
    }
    return false;

}


String getRequest(const char *server, String request) {

    DP("Function: ");
    DPL("getRequest()");

    String result = "";

    // Use WiFiClientSecure class to create TLS connection
    WiFiClientSecure client;
    DP("Connecting to: ");
    DPL(server);

    if (!client.connect(server, httpsPort)) {
        DPL("connection failed");
        return result;
    }

    SetCertificates(client);


    if (client.verifyCertChain(server)) {


        DPL("certificate matches");
        DP("get: ");
        DPL(request);


        client.print(request);


        DPL("request sent");
        DPL("Receiving response");


        while (client.connected()) {
            if (client.find("HTTP/1.1 ")) {
                String status_code = client.readStringUntil('\r');
                DP("Status code: ");
                DPL(status_code);
                if (status_code != "200 OK") {
                    DPL("There was an error");
                    break;
                }
            }
            if (client.find("\r\n\r\n")) {
                DPL("Data:");
            }
            String line = client.readStringUntil('\r');
            DPL(line);
            result += line;
        }

        DPL("closing connection");
        return result;
    } else {
        DPL("certificate doesn't match");
    }
    return result;
}


String postRequest(const char *server, String header, String data) {

    DP("Function: ");
    DPL("postRequest()");


    String result = "";

    // Use WiFiClientSecure class to create TLS connection
    WiFiClientSecure client;
    DP("Connecting to: ");
    DPL(server);

    if (int res=!client.connect(server, httpsPort)) {

        DPL("connection failed with result");
        DPL(res);

        return result;
    }

    SetCertificates(client);
    if (client.verifyCertChain(server)) {

        DPL("certificate matches");
        DP("post: ");
        DPL(header + data);

        client.print(header + data);


        DPL("request sent");
        DPL("Receiving response");


        while (client.connected()) {
            if (client.find("HTTP/1.1 ")) {
                String status_code = client.readStringUntil('\r');
                DP("Status code: ");
                DPL(status_code);
                if ((status_code != "200 OK") && (status_code != "428 Precondition Required")) {
                    DPL("There was an error");
                    break;
                }
            }
            if (client.find("\r\n\r\n")) {
                DPL("Data:");
            }
            String line = client.readStringUntil('\r');
            DPL(line);
            result += line;
        }

        DPL("closing connection");
        return result;
    } else {
        DPL("certificate doesn't match");
    }
    return result;
}


uint8_t request_access_token() {
    DP("Function: ");
    DPL("request_access_token");

    uint8_t my_status = CAL_WAIT_READY;

    String postData = "";
    postData += "&client_id=" + client_id;
    postData += "&refresh_token=" + String(rtcData.refresh_token);
    postData += "&client_secret=1Ta9KtGVZbDb0D1WicO5kz9G";
    postData += "&grant_type=refresh_token";

    String postHeader = "";
    postHeader += ("POST " + token_uri + " HTTP/1.1\r\n");
    postHeader += ("Host: " + String(host) + ":" + String(httpsPort) + "\r\n");
    postHeader += ("Connection: close\r\n");
    postHeader += ("Content-Type: application/x-www-form-urlencoded\r\n");
    postHeader += ("Content-Length: ");
    postHeader += (postData.length());
    postHeader += ("\r\n\r\n");

    String json = postRequest(host, postHeader, postData);
    const size_t bufferSize = JSON_OBJECT_SIZE(5) + 230;
    DynamicJsonBuffer jsonBuffer(bufferSize);
    JsonObject &root = jsonBuffer.parseObject(json);

    if (root.containsKey("access_token")) {

        Serial.println("Refreshing Access Token");

        const char *local_access_token = root["access_token"];
        memset(global_access_token, 0, sizeof(global_access_token));
        memcpy(global_access_token, local_access_token, strlen(local_access_token));

    } else {

        DP("Access Token Refresh Error");
        const char *error_description = root["error_description"];
        DPL(error_description);
        my_status = CAL_PAINT_DONE;

    }

    return my_status;
}


uint8_t poll_authorization_server() {
    DP("Function: ");
    DPL("poll authorization server()");

    String device_code = String(rtcData.device_code);


    String postData = "";
    postData += "&client_id=" + client_id;
    postData += "&client_secret=1Ta9KtGVZbDb0D1WicO5kz9G";
    postData += "&code=" + device_code;
    postData += "&grant_type=http://oauth.net/grant_type/device/1.0";

    String postHeader = "";
    postHeader += ("POST " + token_uri + " HTTP/1.1\r\n");
    postHeader += ("Host: " + String(host) + ":" + String(httpsPort) + "\r\n");
    postHeader += ("Connection: close\r\n");
    postHeader += ("Content-Type: application/x-www-form-urlencoded\r\n");
    postHeader += ("Content-Length: ");
    postHeader += (postData.length());
    postHeader += ("\r\n\r\n");

    const size_t bufferSize = JSON_OBJECT_SIZE(5) + 230;
    DynamicJsonBuffer jsonBuffer(bufferSize);
    String json;

    uint8_t my_status = WIFI_AWAIT_CHALLENGE;
    uint8_t try_count = 0;


    do {

        Serial.print("Polling for authorization server, try #: ");
        Serial.println(try_count);

        json = postRequest(host, postHeader, postData);

        JsonObject &root = jsonBuffer.parseObject(json);

        if (root.containsKey("access_token")) {

            Serial.println("Getting Token");

            const char *local_access_token = root["access_token"];
            memcpy(global_access_token, local_access_token, strlen(local_access_token));

            const char *refresh_token = root["refresh_token"];
            rtcData.status = WIFI_CHECK_ACCESS_TOKEN;
            memset(rtcData.refresh_token, 0, sizeof(rtcData.refresh_token));
            memcpy(rtcData.refresh_token, refresh_token, strlen(refresh_token));
            rtcData.crc32 = calculateCRC32(((uint8_t *) &rtcData) + 4, sizeof(rtcData) - 4);

            EEPROM.put(0, rtcData);
            EEPROM.commit();


            DPL("**** RESULTS in Strings *****");
            DPL(device_code);
            DPL(local_access_token);
            DPL(refresh_token);

            my_status = WIFI_CHECK_ACCESS_TOKEN;

        } else {

            DP("Getting WAIT/ERROR: ");
            const char *error_description = root["error_description"];
            DPL(error_description);
            try_count++;

        }

    } while (my_status == WIFI_AWAIT_CHALLENGE and try_count < 3);

    // Did not work...need to re-initialise device
    if (my_status == WIFI_AWAIT_CHALLENGE) {
        Serial.println("Challenge failed - Restart Initialization");
        const uint8_t invalid_rtc = 0;
        EEPROM.put(0, invalid_rtc);
        EEPROM.commit();
        my_status = WIFI_INITIAL_STATE;
    }

    return my_status;

}


// Linking device, request user-code
const char *request_user_and_device_code() {

    DP("Function: ");
    DPL("authorize()");

    String postData = "";
    postData += "&client_id=" + client_id;
    postData += "&scope=" + scope;

    String postHeader = "";
    postHeader += ("POST " + code_uri + " HTTP/1.1\r\n");
    postHeader += ("Host: " + String(host) + ":" + String(httpsPort) + "\r\n");
    postHeader += ("Connection: close\r\n");
    postHeader += ("Content-Type: application/x-www-form-urlencoded\r\n");
    postHeader += ("Content-Length: ");
    postHeader += (postData.length());
    postHeader += ("\r\n\r\n");

    String json = postRequest(host, postHeader, postData);
    const size_t bufferSize = JSON_OBJECT_SIZE(5) + 230;
    DynamicJsonBuffer jsonBuffer(bufferSize);
    JsonObject &root = jsonBuffer.parseObject(json);

    const char *user_code = root["user_code"];
    const char *device_code = root["device_code"];

    DP("Device Code: ");
    DPL(device_code);
    DP("User Code: ");
    DPL(user_code);

    rtcData.status = WIFI_AWAIT_CHALLENGE;
    memset(rtcData.device_code, 0, sizeof(rtcData.device_code));
    memcpy(rtcData.device_code, device_code, strlen(device_code));
    rtcData.crc32 = calculateCRC32(((uint8_t *) &rtcData) + 4, sizeof(rtcData) - 4);
    EEPROM.put(0, rtcData);
    EEPROM.commit();

    rtcDataStruct myrtc = {0};
    EEPROM.get(0, myrtc);

    DPL("EEPROM-READ");
    DPL(myrtc.status);
    DPL(myrtc.device_code);

    return user_code;

}

#ifdef FALSE

void authorize_old() {

    DP("Function: ");DPL("authorize()");

    if (refresh_token == "") {
        String URL = auth_uri + "?";
        URL += "scope=" + urlencode(scope);
        URL += "&redirect_uri=" + urlencode(redirect_uri);
        URL += "&response_type=" + urlencode(response_type);
        URL += "&client_id=" + urlencode(client_id);
        URL += "&access_type=" + urlencode(access_type);
        DPL("Goto URL: ");
        DPL(URL);DPL();
        Serial.print("Enter code: ");
        global_status = WIFI_AWAIT_CHALLENGE;
    } else {
        global_status = INFO;
    }
}



bool exchange() {

   DP("Function: ");DPL("exchange()");


    if (authorization_code != "") {

        String postData = "";
        postData += "code=" + authorization_code;
        postData += "&client_id=" + client_id;
        postData += "&client_secret=" + client_secret;
        postData += "&redirect_uri=" + redirect_uri;
        postData += "&grant_type=" + String("authorization_code");

        String postHeader = "";
        postHeader += ("POST " + token_uri + " HTTP/1.1\r\n");
        postHeader += ("Host: " + String(host) + ":" + String(httpsPort) + "\r\n");
        postHeader += ("Connection: close\r\n");
        postHeader += ("Content-Type: application/x-www-form-urlencoded\r\n");
        postHeader += ("Content-Length: ");
        postHeader += (postData.length());
        postHeader += ("\r\n\r\n");

        String result = postRequest(host, postHeader, postData);

        global_status = CAL_PAINT_DONE;
        return true;
    } else {
        return false;
    }
}

bool refresh() {

   DP("Function: ");DPL("refresh()");

    if (refresh_token != "") {

        String postData = "";
        postData += "refresh_token=" + refresh_token;
        postData += "&client_id=" + client_id;
        postData += "&client_secret=" + client_secret;
        postData += "&grant_type=" + String("refresh_token");

        String postHeader = "";
        postHeader += ("POST " + token_uri + " HTTP/1.1\r\n");
        postHeader += ("Host: " + String(host) + ":" + String(httpsPort) + "\r\n");
        postHeader += ("Connection: close\r\n");
        postHeader += ("Content-Type: application/x-www-form-urlencoded\r\n");
        postHeader += ("Content-Length: ");
        postHeader += (postData.length());
        postHeader += ("\r\n\r\n");

        String result = postRequest(host, postHeader, postData);

        global_status = CAL_PAINT_DONE;
        return true;
    } else {
        return false;
    }
}

bool info() {

   DP("Function: ");DPL("info()");

    if (access_token != "") {

        String reqHeader = "";
        reqHeader += ("GET " + info_uri + "?access_token=" + access_token + " HTTP/1.1\r\n");
        reqHeader += ("Host: " + String(host) + ":" + String(httpsPort) + "\r\n");
        reqHeader += ("Connection: close\r\n");
        reqHeader += ("\r\n\r\n");
        String result = getRequest(host, reqHeader);

        // need to check for valid token here

        global_status = CAL_WAIT_READY;
    } else {
        global_status = REFRESHING;
        return false;
    }
    return true;
}

String urlencode(String str)
{
    String encodedString = "";
    char c;
    char code0;
    char code1;
    for (unsigned int i = 0; i < str.length(); i++) {
        c = str.charAt(i);
        if (c == ' ') {
            encodedString += '+';
        } else if (isalnum(c)) {
            encodedString += c;
        } else {
            code1 = (c & 0xf) + '0';
            if ((c & 0xf) > 9) {
                code1 = (c & 0xf) - 10 + 'A';
            }
            c = (c >> 4) & 0xf;
            code0 = c + '0';
            if (c > 9) {
                code0 = c - 10 + 'A';
            }
            encodedString += '%';
            encodedString += code0;
            encodedString += code1;
        }
        yield();
    }
    return encodedString;

}
#endif