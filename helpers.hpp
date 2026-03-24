#include <algorithm>
#include "internals/StreamHttpRequest.hpp"
#include "internals/HttpVersion.hpp"
#include <vector>
#include <map>
#include <string>
#include <stdexcept>
#include <utility/wifi_drv.h>
#include <ArduinoHttpServer.h>
#include <cstring>

#pragma once

inline constexpr u_long lerp(u_long a, u_long b, double t) {
	return (u_long)(a * (1 - t) + b * t);
}

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

	static void setR(uint8_t r) {
		WiFiDrv::analogWrite(PIN_R, r);
	}

	static void setG(uint8_t g) {
		WiFiDrv::analogWrite(PIN_G, g);
	}

	static void setB(uint8_t b) {
		WiFiDrv::analogWrite(PIN_B, b);
	}
};

class HttpHelpers {
public:
	HttpHelpers() = delete;

	static String methodToString(ArduinoHttpServer::Method method) {
		switch (method) {
		case ArduinoHttpServer::Method::Get:     return "GET";
		case ArduinoHttpServer::Method::Post:    return "POST";
		case ArduinoHttpServer::Method::Put:     return "PUT";
		case ArduinoHttpServer::Method::Delete:  return "DELETE";
		case ArduinoHttpServer::Method::Head:    return "HEAD";
		case ArduinoHttpServer::Method::Invalid: return "INVALID";
		default: return "INVALID";
		}
	}

	//TEST:
	static std::vector<String> getSegments(String resource) {
		std::vector<String> psegs;
		for (int left = 0; left + 1 < resource.length(); ) {
			int next = resource.indexOf('/', left + 1);
			if (next == -1) {
				psegs.push_back(resource.substring(left));
				break;
			}
			psegs.push_back(resource.substring(left, next));
			left = next;
		}

		if (psegs.empty()) psegs.push_back("/");
		else {
			auto& back = psegs[psegs.size() - 1];
			int idx = back.indexOf('?');
			if (idx != -1) {
				if (back.startsWith("/?")) {
					psegs.back().remove(0, 0);
				}
				else {
					String query = back.substring(idx);
					psegs.back().remove(idx);
					psegs.push_back("/" + query);
				}
			}
		}
		return psegs;
	}

	//TEST:
	static void queryParams(String resource, std::map<String, std::vector<String>>& params) {
		for (int start = resource.indexOf('?') + 1; start < resource.length(); ) {
			int next_eql = resource.indexOf('=', start);
			int next_and = resource.indexOf('&', start);
			if (max(next_eql, next_and) == -1) {
				//one more flag
				String key = resource.substring(start);
				params[key].push_back("");
				break;
			}
			if (next_and == -1) {
				//one more value
				String key = resource.substring(start, next_eql);
				params[key].push_back(resource.substring(next_eql + 1));
				//we ignore any duplicated '=' by skipping to the last one
				break;
			}
			if (next_eql == -1 || next_and < next_eql) {
				//this param is a flag
				String key = resource.substring(start, next_and);
				params[key].push_back("");
				start = next_and + 1;
			}
			else {
				//this param is not a flag
				String key = resource.substring(start, next_eql);
				params[key].push_back(resource.substring(next_eql + 1, next_and));
				start = next_and + 1;
			}
		}
	}
};

class WiFiHelpers {
public:
	static void printWiFiStatus() {
		Serial.print("SSID: ");
		Serial.println(WiFi.SSID());

		IPAddress ip = WiFi.localIP();
		Serial.print("Running at http://");
		Serial.println(ip);
	}

	static inline void ensureWiFiModule() {
		if (WiFi.status() == WL_NO_MODULE) {
			Serial.println("Communication with WiFi module failed!");
			while (true);
		}
	}

	static inline void checkVersion() {
		const char* fv = WiFi.firmwareVersion();
		if (std::strcmp(fv, WIFI_FIRMWARE_LATEST_VERSION) < 0) {
			Serial.print("Please upgrade the firmware from ");
			Serial.print(fv);
			Serial.print(" to ");
			Serial.println(WIFI_FIRMWARE_LATEST_VERSION);
		}
	}
};