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

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;
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

	//pinMode(1, OUTPUT);
	pinMode(PIN_BUTTON, INPUT_PULLUP);
	servo.attach(PIN_SERVO);
	pinMode(LED_BUILTIN, OUTPUT);
	MultiLED::init();

	Serial.println("Access Point Web Server");

	if (WiFi.status() == WL_NO_MODULE) {
		Serial.println("Communication with WiFi module failed!");
		while (true);
	}
	String fv = WiFi.firmwareVersion();
	if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
		Serial.print("Please upgrade the firmware from ");
		Serial.print(fv);
		Serial.print(" to ");
		Serial.println(WIFI_FIRMWARE_LATEST_VERSION);
	}
	//WiFi.config(IPAddress(192, 168, 4, 1));
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
	printWiFiStatus();
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

void printWiFiStatus() {
	Serial.print("SSID: ");
	Serial.println(WiFi.SSID());

	IPAddress ip = WiFi.localIP();
	Serial.print("Running at http://");
	Serial.println(ip);
}

void handleClient(WiFiClient client) {
	ArduinoHttpServer::StreamHttpRequest<2048> request(client);
	bool success = request.readRequest();

	if (!success) {
		client.println("HTTP/1.1 400 Bad Request");
		client.println();
		return;
	}

	//TODO: Guard against bad versioning

	auto method = request.getMethod();
	auto body = String(request.getBody());
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
		//else if (endpoint[0].equalsIgnoreCase("/H")) handle_high(client);
		//else if (endpoint[0].equalsIgnoreCase("/L")) handle_low(client);
		//else if (endpoint[0].equalsIgnoreCase("/setRGB")) handle_setrgb(client, query);
		//else if (endpoint[0].equalsIgnoreCase("/setServo")) handle_setservo(client, query);
		//else if (endpoint[0].equalsIgnoreCase("/getBtn")) handle_getbtn(client);
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
	client.println();
	client.print("TODO: Put something here");
	client.println();
}
void handle_lock(WiFiClient client) {
	servoLock();
	client.println("HTTP/1.1 204 No Content");
	client.println();
}
void handle_unlock(WiFiClient client) {
  servoClear();
	//if (state == DoorState::LOCKED) {
  client.println("HTTP/1.1 202 Accepted");
  client.println();
	// }
	// else {
	// 	client.print("HTTP/1.1 ");
	// 	client.println((state == DoorState::CLEAR ? "202 Accepted" : "304 Not Modified"));
	// 	client.println();
	// }
}
void handle_high(WiFiClient client) {
	client.println("HTTP/1.1 204 No Content");
	client.println();
	digitalWrite(LED_BUILTIN, HIGH);
}
void handle_low(WiFiClient client) {
	client.println("HTTP/1.1 204 No Content");
	client.println();
	digitalWrite(LED_BUILTIN, LOW);
}
void handle_getbtn(WiFiClient client) {
	client.println("HTTP/1.1 200 OK");
	client.println();
	client.println(!digitalRead(0));
}
void handle_setrgb(WiFiClient client, std::map<String, std::vector<String>> query) {
	uint flag_r = 0, flag_g = 0, flag_b = 0;

	flag_r += query.count("r");
	flag_r += query.count("R");
	flag_g += query.count("g");
	flag_g += query.count("G");
	flag_b += query.count("b");
	flag_b += query.count("B");

	if (flag_r > 1 || flag_g > 1 || flag_b > 1) {
		client.println("HTTP/1.1 400 Bad Request");
		client.println();
		client.println("Duplicate query parameters are invalid");
		return;
	}
	if (flag_r + flag_g + flag_b <= 0) {
		client.println("HTTP/1.1 204 No Content");
		client.println();
		return;
	}

	client.println("HTTP/1.1 200 OK");
	client.println();

	String res_buff = "";
	if (flag_r) {
		String val_r;
		if (query.count("r") > 0) {
			val_r = query["r"].front();
		}
		else val_r = query["R"].front();
		res_buff += "set red to " + val_r + "\n";
		MultiLED::setR((uint8_t)val_r.toInt());
	}
	if (flag_g) {
		String val_g;
		if (query.count("g") > 0) {
			val_g = query["g"].front();
		}
		else val_g = query["G"].front();
		res_buff += "set green to " + val_g + "\n";
		MultiLED::setG((uint8_t)val_g.toInt());
	}
	if (flag_b) {
		String val_b;
		if (query.count("b") > 0) {
			val_b = query["b"].front();
		}
		else val_b = query["B"].front();
		res_buff += "set blue to " + val_b + "\n";
		MultiLED::setB((uint8_t)val_b.toInt());
	}

	client.println(res_buff);
}
void handle_setservo(WiFiClient client, std::map<String, std::vector<String>> query) {
	if (query.count("dir") != 1) {
		client.println("HTTP/1.1 400 Bad Request");
		client.println();
	}
	auto raw = query["dir"].front();
	double parsed = raw.toDouble();
	/*if(parsed == 0){
	  client.println("HTTP/1.1 400 Bad Request");
	  client.println();
	  return;
	}*/
	servoSetAndRefresh(lerp(600uL, 1800uL, parsed));
	client.println("HTTP/1.1 204 No Content");
	client.println();
}
#pragma endregion

void PWM(uint8_t pin, float period, float width) {
	digitalWrite(pin, HIGH);
	delayMicroseconds(width * 1000);
	digitalWrite(pin, LOW);
	delayMicroseconds((period - width) * 1000);
}
void PWM(uint8_t pin, unsigned long period, unsigned long width, unsigned long t) {
	u_long mt = t % period;
	digitalWrite(pin, mt > width ? LOW : HIGH);
}
int PWM(unsigned long period, unsigned long width, unsigned long t) {
	u_long mt = t % period;
	return (int)(mt > width);
}