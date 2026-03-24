#undef abs

#include <SPI.h>
#include <WiFiNINA.h>
#include <Servo.h>
#include "arduino_secrets.h"
#include "helpers.hpp"
#include "config.hpp"

const char SSID[] = SECRET_SSID;
const char PASS[] = SECRET_PASS;

enum DoorState : uint8_t {
    LOCKED = 0,
    CLEAR = 1,
    UNLOCKED = 2,
};

WiFiServer server(80);
Servo servo;

DoorState doorState = LOCKED;
int wifiStatus = WL_IDLE_STATUS;
unsigned long servoTimestamp = 0;
unsigned long pwmWidth = SERVO_UNLOCK_US;

static void servoWriteAndHold(unsigned long pulseUs) {
    servoTimestamp = millis();
    if (!servo.attached()) {
        servo.attach(PIN_SERVO);
    }
    servo.writeMicroseconds(pulseUs);
    pwmWidth = pulseUs;
}

static void setLocked() {
    servoWriteAndHold(SERVO_LOCK_US);
    doorState = LOCKED;
    MultiLED::set(5, 0, 0);
}

static void setClear() {
    servoWriteAndHold(pwmWidth);
    if (doorState == LOCKED) {
        doorState = CLEAR;
        MultiLED::set(5, 5, 0);
    }
}

static void setUnlocked() {
    servoWriteAndHold(SERVO_UNLOCK_US);
    doorState = UNLOCKED;
    MultiLED::set(0, 5, 0);
}

static void sendResponse(
    WiFiClient& client,
    int statusCode,
    const char* statusText,
    const char* contentType = nullptr,
    const char* body = nullptr
) {
    client.print("HTTP/1.1 ");
    client.print(statusCode);
    client.print(' ');
    client.println(statusText);

    if (contentType != nullptr) {
        client.print("Content-Type: ");
        client.println(contentType);
    }

    if (body != nullptr) {
        client.print("Content-Length: ");
        client.println(strlen(body));
    }
    else {
        client.println("Content-Length: 0");
    }

    client.println("Connection: close");
    client.println();

    if (body != nullptr) {
        client.print(body);
    }
}

static bool readRequestLine(
    WiFiClient& client,
    char* method,
    size_t methodSize,
    char* path,
    size_t pathSize
) {
    char line[128];
    size_t len = client.readBytesUntil('\n', line, sizeof(line) - 1);
    if (len == 0) {
        return false;
    }

    line[len] = '\0';

    if (len > 0 && line[len - 1] == '\r') {
        line[len - 1] = '\0';
    }

    const char* firstSpace = strchr(line, ' ');
    if (firstSpace == nullptr) {
        return false;
    }

    const char* secondSpace = strchr(firstSpace + 1, ' ');
    if (secondSpace == nullptr) {
        return false;
    }

    size_t methodLen = static_cast<size_t>(firstSpace - line);
    size_t pathLen = static_cast<size_t>(secondSpace - (firstSpace + 1));

    if (methodLen == 0 || methodLen >= methodSize || pathLen == 0 || pathLen >= pathSize) {
        return false;
    }

    memcpy(method, line, methodLen);
    method[methodLen] = '\0';

    memcpy(path, firstSpace + 1, pathLen);
    path[pathLen] = '\0';

    char* query = strchr(path, '?');
    if (query != nullptr) {
        *query = '\0';
    }

    return true;
}

static void discardHeaders(WiFiClient& client) {
    char line[128];

    while (true) {
        size_t len = client.readBytesUntil('\n', line, sizeof(line) - 1);
        if (len == 0) {
            break;
        }

        line[len] = '\0';

        if ((len == 1 && line[0] == '\r') || (len == 0)) {
            break;
        }
    }
}

static void handleClient(WiFiClient& client) {
    char method[8];
    char path[32];

    if (!readRequestLine(client, method, sizeof(method), path, sizeof(path))) {
        sendResponse(client, 400, "Bad Request", "text/plain", "Bad Request");
        return;
    }

    discardHeaders(client);

    Serial.print(method);
    Serial.print(' ');
    Serial.println(path);

    if (strcmp(method, "GET") != 0) {
        sendResponse(client, 405, "Method Not Allowed", "text/plain", "Method Not Allowed");
        return;
    }

    if (strcmp(path, "/") == 0) {
        static constexpr char BODY[] =
            "Door controller\n"
            "GET /lock\n"
            "GET /unlock\n";
        sendResponse(client, 200, "OK", "text/plain", BODY);
        return;
    }

    if (strcmp(path, "/lock") == 0) {
        setLocked();
        sendResponse(client, 204, "No Content");
        return;
    }

    if (strcmp(path, "/unlock") == 0) {
        setClear();
        sendResponse(client, 202, "Accepted");
        return;
    }

    sendResponse(client, 404, "Not Found", "text/plain", "Not Found");
}

void setup() {
    Serial.begin(9600);

    pinMode(PIN_BUTTON, INPUT_PULLUP);
    pinMode(LED_BUILTIN, OUTPUT);

    servo.attach(PIN_SERVO);
    MultiLED::init();

    WiFiHelpers::ensureWiFiModule();
    WiFiHelpers::checkVersion();

    Serial.print("Joining: ");
    Serial.println(SSID);

    wifiStatus = WiFi.begin(SSID, PASS);
    if (wifiStatus != WL_CONNECTED) {
        Serial.println("Joining network failed");
        while (true) {}
    }

    delay(1000);

    server.begin();
    WiFiHelpers::printWiFiStatus();
    setLocked();
}

void loop() {
    int currentStatus = WiFi.status();
    if (wifiStatus != currentStatus) {
        wifiStatus = currentStatus;
        if (wifiStatus == WL_CONNECTED) {
            Serial.println("WiFi Connected");
        }
        else {
            Serial.println("WiFi Disconnected");
        }
    }

    WiFiClient client = server.available();
    if (client) {
        handleClient(client);
        client.stop();
    }

    if (doorState == CLEAR && digitalRead(PIN_BUTTON) == LOW) {
        setUnlocked();
    }

    if (servo.attached() && (millis() - servoTimestamp > SERVO_HOLD_MS)) {
        servo.detach();
    }
}