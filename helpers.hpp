#pragma once

#include <WiFiNINA.h>
#include <utility/wifi_drv.h>
#include <cstring>

class MultiLED {
public:
    MultiLED() = delete;

    static constexpr uint8_t PIN_R = 25;
    static constexpr uint8_t PIN_G = 26;
    static constexpr uint8_t PIN_B = 27;

    static void init() {
        WiFiDrv::pinMode(PIN_R, OUTPUT);
        WiFiDrv::pinMode(PIN_G, OUTPUT);
        WiFiDrv::pinMode(PIN_B, OUTPUT);
    }

    static void set(uint8_t r, uint8_t g, uint8_t b) {
        WiFiDrv::analogWrite(PIN_R, r);
        WiFiDrv::analogWrite(PIN_G, g);
        WiFiDrv::analogWrite(PIN_B, b);
    }
};

class WiFiHelpers {
public:
    WiFiHelpers() = delete;

    static void printWiFiStatus() {
        Serial.print("SSID: ");
        Serial.println(WiFi.SSID());

        IPAddress ip = WiFi.localIP();
        Serial.print("Running at http://");
        Serial.println(ip);
    }

    static void ensureWiFiModule() {
        if (WiFi.status() == WL_NO_MODULE) {
            Serial.println("Communication with WiFi module failed!");
            while (true) {}
        }
    }

    static void checkVersion() {
        const char* fv = WiFi.firmwareVersion();
        if (std::strcmp(fv, WIFI_FIRMWARE_LATEST_VERSION) < 0) {
            Serial.print("Please upgrade the firmware from ");
            Serial.print(fv);
            Serial.print(" to ");
            Serial.println(WIFI_FIRMWARE_LATEST_VERSION);
        }
    }
};