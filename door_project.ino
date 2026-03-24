//TODO: remove dynamic memory
//TODO: have the system idle
//https://docs.arduino.cc/tutorials/generic/sleep-rtc-alarm/
//https://docs.arduino.cc/tutorials/mkr-wifi-1010/mkr-battery-app-note/

#undef abs //conflict with std

#include <vector>
#include <map>
#include <SPI.h>
#include <WiFiNINA.h>
#include <Servo.h>
#include <ArduinoHttpServer.h>
#include "arduino_secrets.h"
#include "helpers.hpp"
#include "config.hpp"

using HttpMethod = ArduinoHttpServer::Method;

const char ssid[] = SECRET_SSID;
const char pass[] = SECRET_PASS;
int status = WL_IDLE_STATUS;

enum DoorState : uint8_t {
	LOCKED = 0,
	CLEAR = 1, //Ok to unlock but not yet unlocked
	UNLOCKED = 2,
};
DoorState state = DoorState::LOCKED;

WiFiServer server(80);
Servo servo;

unsigned long pwm_width = 600; //1000 * [0.6, 2.4] = [600, 2400]
uint32_t servo_timestamp;

void setup() {
	Serial.begin(9600);

	pinMode(PIN_BUTTON, INPUT_PULLUP);
	servo.attach(PIN_SERVO);
	pinMode(LED_BUILTIN, OUTPUT);
	MultiLED::init();

	WiFiHelpers::ensureWiFiModule();
	WiFiHelpers::checkVersion();
	
	Serial.print("Joining: ");
	Serial.println(ssid);
	status = WiFi.begin(ssid, pass);
	if (status != WL_CONNECTED) {
		Serial.println("Joining network failed");
		while (true);
	}
	delay(1000);
	Serial.println("Ready for connections");
	server.begin();
	WiFiHelpers::printWiFiStatus();
	servoLock();
}

inline void servoSetAndRefresh(unsigned long val) {
  servo_timestamp = millis();
  servo.attach(PIN_SERVO);
  servo.writeMicroseconds(val);
  pwm_width = val;
}

void servoLock() {
  servoSetAndRefresh(SERVO_LOCK_US);
	state = DoorState::LOCKED;
	MultiLED::set(5, 0, 0);
}
void servoClear() {
  servoSetAndRefresh(pwm_width);
  if (state == DoorState::LOCKED)
  {
    state = DoorState::CLEAR;
    MultiLED::set(5, 5, 0);
  }
}
void servoUnlock() {
  servoSetAndRefresh(SERVO_UNLOCK_US);
	state = DoorState::UNLOCKED;
	MultiLED::set(0, 5, 0);
}

void loop() {
	if (status != WiFi.status()) {
		status = WiFi.status();
		if (status == WL_CONNECTED) {
			Serial.println("WiFi Connected");
		}
		else {
			Serial.println("WiFi Disconnected");
		}
	}
	WiFiClient client = server.available();
	if (client) {
		Serial.println("-----------");
		handleClient(client);
		client.stop();
	}
	if (state == DoorState::CLEAR && !digitalRead(PIN_BUTTON)) {
		servoUnlock();
	}

  if (millis() - servo_timestamp > SERVO_HOLD_MS){
    servo.detach();
  }
}

void handleClient(WiFiClient client) {
	ArduinoHttpServer::StreamHttpRequest<2048> request(client);
	bool success = request.readRequest();

	if (!success) {
		client.println("HTTP/1.1 400 Bad Request\n");
		return;
	}

	//TODO: Guard against bad versioning

	auto method    = request.getMethod();
	auto body      = String(request.getBody());
	auto& resource = request.getResource().toString();

	auto endpoint = HttpHelpers::getSegments(resource);

	std::map<String, std::vector<String>> params;
	if (!endpoint.empty() && endpoint.back().startsWith("/?")) {
		String query = endpoint[endpoint.size() - 1];
		if (endpoint.size() > 1)
			endpoint.pop_back();
		else endpoint.back().remove(1);
		HttpHelpers::queryParams(query, params);
	}

#pragma region print method
	Serial.print(HttpHelpers::methodToString(method));
	Serial.print(" ");
	Serial.println(resource);
#pragma endregion

#pragma region print path
	Serial.print("Path segments: ");
	Serial.println(endpoint.size());
	for (int i = 0; i < endpoint.size(); i++) {
		Serial.print("[");
		Serial.print(i);
		Serial.print("] - ");
		Serial.println(endpoint[i]);
	}
	Serial.println();
#pragma endregion

#pragma region print query
	if (params.size() > 0) {
		Serial.println("Query: ");
		for (const auto& [key, vals] : params) {
			Serial.print(key);
			Serial.print(": [");
			for (size_t i = 0; i < vals.size(); ++i) {
				Serial.print(vals[i]);
				if (i < vals.size() - 1) Serial.print(", ");
			}
			Serial.println("]");
		}
		Serial.println();
	}
#pragma endregion

#pragma region print body
	Serial.print("Payload length: ");
	Serial.println(body.length());
	if (body.length() > 0)
		Serial.println(body);
	Serial.println();
#pragma endregion

	endpointSwitch(client, method, endpoint, params, body);
}

void endpointSwitch(WiFiClient client, HttpMethod method, std::vector<String> endpoint, std::map<String, std::vector<String>> query, String body) {
	bool flag = true;
	//OPTIMIZE:
	if (endpoint.size() == 1) {
		if (endpoint[0].equals("/")) handle_root(client);
		else if (endpoint[0].equalsIgnoreCase("/lock")) handle_lock(client);
		else if (endpoint[0].equalsIgnoreCase("/unlock")) handle_unlock(client);
		else flag = false;
	}
	if (!flag) {
		client.println("HTTP/1.1 404 Not Found");
		client.println();
	}
	return;
}

#pragma region endpoint_handlers
void handle_root(WiFiClient client) {
	client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println("TODO: Put something here\n");
}
void handle_lock(WiFiClient client) {
	servoLock();
	client.println("HTTP/1.1 204 No Content");
  client.println("");
}
void handle_unlock(WiFiClient client) {
  servoClear();
  client.println("HTTP/1.1 202 Accepted");
  client.println();
}

#pragma endregion