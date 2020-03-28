//
// Created by development on 05.01.19.
//


#include <WString.h>
#include "HardwareSerial.cpp"

#define USING_AXTLS

#include <ESP8266WiFi.h>
#include <WiFiClientSecureAxTLS.h>

using namespace axTLS;

#include "cacert.h"
#include <time.h>
#include "ArduinoJson.h"
#include "main_esp8266_wifi.h"
#include "oauth.h"
#include "support.h"

// #include <CertStoreBearSSL.h>

// OAUTH2 Client credentials
static const String client_id = "88058113591-7ek2km1rt9gsjhlpb9fuckhl8kpnllce.apps.googleusercontent.com";
static const String scope = "https://www.googleapis.com/auth/calendar.readonly";
//const String scope = "https://www.googleapis.com/auth/calendar";
static const String auth_uri = "https://accounts.google.com/o/oauth2/auth";
static const String code_uri = "https://accounts.google.com/o/oauth2/device/code";
static const String info_uri = "/oauth2/v3/tokeninfo";
static const String token_uri = "/oauth2/v4/token";
static const char *host = "www.googleapis.com";
const static int httpsPort = 443;



bool SetupMyWifi(const char *ssid, const char *password) {
    CP("Connecting to: ");
    CPL(ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    uint16_t try_count = 100;

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        DP(".");
        try_count--;
        if (try_count == 0) break;
    }

    if (try_count == 0) {
        CPL("");
        CPL("WIFI Connection ERROR");
        return true;
    }

    CPL("");
    CP("WiFi connected with IP address:");
    CPL(WiFi.localIP());


    return false;
}


void SetupTimeSNTP(tm *timeinfo) {
    // Synchronize time useing SNTP. This is necessary to verify that
// the TLS certificates offered by the server are currently valid.

    CPL("Setting time using SNTP");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    time_t now = time(nullptr);
    while (now < 8 * 3600 * 2) {
        delay(500);
        DP(".");
        now = time(nullptr);
    }
    DPL("");

    gmtime_r(&now, timeinfo);
    timeinfo->tm_hour = timeinfo->tm_hour + TIME_ZONE;
    mktime(timeinfo);

    CP("*** Current time: ");
    CPL(asctime(timeinfo));


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

bool CheckCertifcates() {
    WiFiClientSecure client;

    if (!client.connect(host, httpsPort)) {
        ErrorToDisplay("CheckCertificate: Connection failed");
        return true;
    }

    SetCertificates(client);
    if (!client.verifyCertChain(host)) {
        ErrorToDisplay("Certificates dont match!");
        return true;
    }

    DPL("Certifcates match");

    return false;
}

bool sendHttpRequest(WiFiClientSecure client, const String request) {

    DPL("sendHttpRequest");

    size_t res=client.print(request);
    DP("Request->");DP(request);DP("<- Size:");DPL(res);
    DP("Request sent, checking response with heap:");CPL(ESP.getFreeHeap());

    return true;
}

String readHttpRequest(WiFiClientSecure client, bool b_swser) {

    DPL("readHttpRequest");
    // Reading Headers ****************************
    String str_header;
    uint32 length = 0;
    bool b_chunked = false;
    long http_code = 0;
    String result="";

    while (true) {
        str_header = client.readStringUntil('\n');
//        DP("->");DP(str_header);DPL("<-");

        if (str_header.indexOf("Transfer-Encoding: chunked") != -1) {
            b_chunked = true;
            DPL("Header: Found Chunked");
        }

        if (str_header.indexOf("Content-Length:") != -1) {
            length = str_header.substring(str_header.indexOf(":") + 1).toDouble();
            DP("Header: Found Length:");
            DPL(length);
        }

        if (str_header.indexOf("HTTP/1") != -1) {
            http_code = str_header.substring(str_header.indexOf(" ") + 1, str_header.indexOf(" ") + 4).toInt();
            DP("Header: HTTP-Status:");
            DPL(http_code);
        }

        if (str_header == "\r") {
            Serial.println("DONE: headers received");
            break;
        }
    }

    if (http_code == 0) {
        result = "CN_ERROR: No HTTP/1 Found in Header!";
        DPL(result);
        return result;

    }

    if ((http_code != 200) && (http_code != 428)) {
        result = "CN_ERROR -  HTTP:" + String(http_code);
        DPL(result);
        return result;
    }


    DPL("******** Receiving Data :");

    if (b_chunked) {
        DPL("*** Chunked Reading ***");
        while (true) {
            String line = client.readStringUntil('\r\n');
            DP("Chunk-Len:");
            DP(line);
            DPL("<-");

            char chr_line[20];
            line.toCharArray(chr_line, 20);
            long len = strtol(chr_line, NULL, 16);
            DP("Chunk-Len LongInt:");
            DPL(len);
            DP("BUFFER->");
            if (len != 0) {

                if (!b_swser) {
                    char *my_buffer = (char *) malloc(len + 1);
                    memset(my_buffer, 0, len + 1);
                    client.readBytes(my_buffer, len);
                    DP(my_buffer);
                    result = result + String(my_buffer);
                    free(my_buffer);
                } else {
                    char my_buffer[1];
                    for (uint32 i = 0; i < len; i++) {
                        client.readBytes(my_buffer, 1);
                        swSer.write(my_buffer[0]);
                        DP(my_buffer[0]);
                    }
                    uint8_t v = 0;
                    swSer.write(v);
                }
                client.readStringUntil('\r\n');
            } else {
                break;
            }
            DPL("<-BUFFER");

        }
    } else if (length != 0) {
        DPL("*** Content-Length Reading ***");
        DP("BUFFER->");
        if (!b_swser) {
            char *my_buffer = (char *) malloc(length + 1);
            memset(my_buffer, 0, length + 1);
            client.readBytes(my_buffer, length);
            result = String(my_buffer);
            DP(result);
        } else {
            char my_buffer[1];
            for (uint32 i = 0; i < length; i++) {
                client.readBytes(my_buffer, 1);
                swSer.write(my_buffer[0]);
                DP(my_buffer[0]);
            }
            uint8_t v = 0;
            swSer.write(v);
        }
        DPL("<-BUFFER");


    } else {
        DPL("*** Connected Reading (most slow) ***");
        DP("BUFFER->");
        if (!b_swser) {
            while (client.connected()) {
                String line = client.readStringUntil('\r');
                DP(line);
                result += line;
            }
        } else {

            char buffer[1];
            size_t b;

            while (true) {
                b = client.readBytesUntil('\r', buffer, (size_t) 1);
                DP(buffer[0]);
                if (b == 1) {
                    swSer.write((uint8_t) buffer[0]);
                } else {
                    uint8_t v = 0;
                    swSer.write(v);
                    break;
                }
            }
        }
        DPL("<-BUFFER");
    }

    DPL("closing connection");
    client.stop();

    return result;

}


bool calendarGetRequest(char *request) {

    bool result = false;

    CP("calenderGetRequest, heap is: ");CPL(ESP.getFreeHeap());

    ssize_t bufsz = snprintf(NULL, 0,
                             "GET %saccess_token=%s HTTP/1.1\r\nHost: www.googleapis.com:443\r\nConnection: close\r\n\r\n\r\n",
                             request, global_access_token);

    char *full_request = (char *) malloc(bufsz + 1);
    DP("Full Request Size:");DPL(bufsz);

    sprintf(full_request,
            "GET %saccess_token=%s HTTP/1.1\r\nHost: www.googleapis.com:443\r\nConnection: close\r\n\r\n\r\n",
            request, global_access_token);

    DPL(full_request);

    // Use WiFiClientSecure class to create TLS connection

    WiFiClientSecure client;

    if (!client.connect(host, httpsPort)) {
        ErrorToDisplay("CalGetRequest: Connection failed");
        return false;
    }

    //*** Send Request to Google to get calendar information, error message is done in the function
    if (!sendHttpRequest(client, full_request) ) return false;

    //*** Write to calendar and  Check Response *************************************************
    String res = readHttpRequest(client, true);

    if (res.indexOf("CN_ERROR") != -1) {
        char tmp_err[150] = {0};
        res = "CalGetReq-Send:" + res;
        res.toCharArray(tmp_err, res.length()+1);
        ErrorToDisplay(tmp_err);
        return false;
    }

    client.stop();

    return true;
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
        client.stop();
        return result;
    } else {
        DPL("certificate doesn't match");
    }
    return result;
}


String postRequest(const char *server, String header, String data) {

    DP("Function: ");
    DPL("postRequest()");


    WiFiClientSecure client;
    String result = "";

    if (int res = !client.connect(server, httpsPort)) {
        DPL("connection failed with result");
        DPL(res);
        return result;
    }

    // Use WiFiClientSecure class to create TLS connection

    DP("Connecting to: ");
    DPL(server);


    SetCertificates(client);
    if (client.verifyCertChain(server)) {

        DPL("certificate matches");
        DP("post: ");
        DPL(header + data);

        client.print(header + data);

        DPL("request sent");
        DPL("Receiving response");
        // Reading Headers ****************************
        while (client.connected()) {
            DP(".");
            String line = client.readStringUntil('\n');
            DPL(line);
            if (line == "\r") {
                Serial.println("headers received");
                break;
            }
        }
//            String line = client.readStringUntil('\n');

        // Reading Body (chunked)  ****************************
        // https://en.wikipedia.org/wiki/Chunked_transfer_encoding

        while (true) {
            String line = client.readStringUntil('\r\n');
            DP("Chunk-Len:");
            DP(line);
            DPL("<-");

            char chr_line[20];
            line.toCharArray(chr_line, 20);
            long len = strtol(chr_line, NULL, 16);
            DP("Chunk-Len LongInt:");
            DPL(len);

            if (len != 0) {
                char *my_buffer = (char *) malloc(len + 1);
                memset(my_buffer, 0, len + 1);
                client.readBytes(my_buffer, len);
                DP("BUFFER->");
                DP(my_buffer);
                DPL("<-BUFFER");
                result = result + String(my_buffer);
                free(my_buffer);
                client.readStringUntil('\r\n');
            } else {
                break;
            }

        }
        DP("->");
        DP(result);
        DPL("<-");

        DPL("closing connection");
        client.stop();

    } else {
        DPL("certificate doesn't match");
    }
    return result;
}


uint8_t request_access_token() {
    DPL("request_access_token");

    uint8_t my_status = CAL_PAINT_UPDATE;

    String postData = "";
    postData += "client_id=" + client_id;
    postData += "&refresh_token=" + String(rtcOAuth.refresh_token);
    postData += "&client_secret=1Ta9KtGVZbDb0D1WicO5kz9G";
    postData += "&grant_type=refresh_token";

    String postHeader = "";
    postHeader += ("POST " + token_uri + " HTTP/1.1\r\n");
    postHeader += ("Host: " + String(host) + ":" + String(httpsPort) + "\r\n");
    postHeader += ("Accept: */*\r\n");
    postHeader += ("Content-Type: application/x-www-form-urlencoded\r\n");
    postHeader += ("Content-Length: ");
    postHeader += (postData.length());
    postHeader += ("\r\n\r\n");

    WiFiClientSecure client;

    if (!client.connect(host, httpsPort)) {
        ErrorToDisplay("Req.Acc.Token: Connection failed");
        return ESP_SEND_ERROR_MSG;
    }

    //*** Send Request to Google to get calendar information, error message is done in the function
    if (!sendHttpRequest(client, postHeader+postData) ) return ESP_SEND_ERROR_MSG;

    String json = readHttpRequest(client,false);

    if (json.indexOf("CN_ERROR") != -1) {
        char tmp_err[100] = {0};
        json = "RequestToken:" + json;
        json.toCharArray(tmp_err,json.length()+1);
        ErrorToDisplay(tmp_err);
        return ESP_SEND_ERROR_MSG;
    }

    const size_t bufferSize = JSON_OBJECT_SIZE(5) + 1024;
    DynamicJsonDocument jsonBuffer(bufferSize);
//    JsonObject &root = jsonBuffer.parseObject(json);
    deserializeJson(jsonBuffer, json);
    JsonObject root = jsonBuffer.as<JsonObject>();

    if (root.containsKey("access_token")) {

        DPL("Refreshing Access Token");
        const char *local_access_token = root["access_token"];
        global_access_token = (char *) calloc(sizeof(char), strlen(local_access_token) + 1);
        memcpy(global_access_token, local_access_token, strlen(local_access_token));

    } else {

        DPL("Access Token Refresh Error");
        ErrorToDisplay("Access Token Refresh");
        my_status = ESP_SEND_ERROR_MSG;

    }
    client.stop();
    return my_status;
}


uint8_t poll_authorization_server() {
    DP("Function: ");
    DPL("poll authorization server()");

    String device_code = String(rtcOAuth.device_code);

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

    const size_t bufferSize = JSON_OBJECT_SIZE(5) + 1024;
    DynamicJsonDocument jsonBuffer(bufferSize);

    String json;

    uint8_t my_status = WIFI_AWAIT_CHALLENGE;
    uint8_t try_count = 0;

    WiFiClientSecure client;

#define MAX_TRY_COUNT 6

    do {

        DP("Polling for authorization server, try #: ");
        DP(try_count);
        DP(" of ");
        DPL(MAX_TRY_COUNT);

        if (!client.connect(host, httpsPort)) {
            ErrorToDisplay("PollServer: Connection failed");
            return ESP_SEND_ERROR_MSG;
        }

        //*** Send Request to Google to get calendar information, error message is done in the function
        size_t res=client.print(postHeader+postData);
        DP("Request->");DP(postHeader+postData);DP("<- Size:");DPL(res);

        json = readHttpRequest(client,false);

        if (json.indexOf("CN_ERROR") != -1) {
            char tmp_err[100] = {0};
            json = "PollAuthorization:" + json;
            json.toCharArray(tmp_err,json.length()+1);
            ErrorToDisplay(tmp_err);
            return ESP_SEND_ERROR_MSG;
        }

        deserializeJson(jsonBuffer, json);
        JsonObject root = jsonBuffer.as<JsonObject>();

        if (root.containsKey("access_token")) {

            const char *local_access_token = root["access_token"];

            const char *refresh_token;
            if(root.containsKey("refresh_token")) {
                DPL("Found Refresh Token");
               refresh_token = root["refresh_token"];
            } else {
                ErrorToDisplay("PollServer: RefreshToken");
                return ESP_SEND_ERROR_MSG;
            }
            global_access_token = (char *) calloc(sizeof(char), strlen(local_access_token) + 1);
            memcpy(global_access_token, local_access_token, strlen(local_access_token));


            // Update RTC with Refresh Token
            rtcOAuth.status = WIFI_CHECK_ACCESS_TOKEN;
            memset(rtcOAuth.refresh_token, 0, sizeof(rtcOAuth.refresh_token));
            memcpy(rtcOAuth.refresh_token, refresh_token, strlen(refresh_token));
            RTC_OAuthWrite();

            DPL("**** RESULTS in Strings *****");
            DP("Device Code:");DPL(device_code);
            DP("Access Token:");DPL(local_access_token);
            DP("Refresh Token:");DPL(refresh_token);

            my_status = WIFI_CHECK_ACCESS_TOKEN;
            client.stop();

        } else {
            client.stop();
            DP("Getting WAIT/ERROR: ");
            const char *error_description = root["error_description"];
            DPL(error_description);
            try_count++;
            delay(30000);
        }



    } while (my_status == WIFI_AWAIT_CHALLENGE and try_count < MAX_TRY_COUNT);

    // Did not work...need to re-initialise device
    if (my_status == WIFI_AWAIT_CHALLENGE) {

        DPL("Challenge failed - Restart Initialization");
        memset(&rtcOAuth, 0, sizeof(rtcOAuth));
        rtcOAuth.status = WIFI_INITIAL_STATE;
        RTC_OAuthWrite();
        ErrorToDisplay("User Code not entered!");
        return ESP_SEND_ERROR_MSG;
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

    WiFiClientSecure client;

    if (!client.connect(host, httpsPort)) {
        ErrorToDisplay("Req.Device.Code: Connection failed");
        return 0;
    }

    //*** Send Request to Google to get calendar information, error message is done in the function
    if (!sendHttpRequest(client, postHeader+postData) ) return 0;

    String json = readHttpRequest(client,false);

    if (json.indexOf("CN_ERROR") != -1) {
        char tmp_err[100] = {0};
        json = "Req.Device.Code:" + json;
        json.toCharArray(tmp_err,json.length()+1);
        ErrorToDisplay(tmp_err);
        return 0;
    }

    const size_t bufferSize = JSON_OBJECT_SIZE(5) + 1024;

    DynamicJsonDocument jsonBuffer(bufferSize);
    deserializeJson(jsonBuffer, json);
    JsonObject root = jsonBuffer.as<JsonObject>();

    const char *user_code = root["user_code"];
    const char *device_code = root["device_code"];

    DP("Device Code: ");
    DPL(device_code);
    DP("User Code: ");
    DPL(user_code);

    rtcOAuth.status = WIFI_AWAIT_CHALLENGE;
    memset(rtcOAuth.device_code, 0, sizeof(rtcOAuth.device_code));
    memcpy(rtcOAuth.device_code, device_code, strlen(device_code));
    RTC_OAuthWrite();
    client.stop();
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
        DP("Enter code: ");
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