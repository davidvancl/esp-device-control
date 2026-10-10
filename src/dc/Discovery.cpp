#include "Discovery.h"

#include "Config.h"
#include "Platform.h"

#if defined(ESP8266)
#include <ESP8266mDNS.h>
#else
#include <ESPmDNS.h>
#endif

namespace DeviceControl {
namespace Discovery {

namespace {

const char* deviceName = "";
const char* deviceType = "";
const char* deviceId = "";
const char* deviceFirmware = "";
bool announced = false;
uint32_t lastRetry = 0;

void announce() {
  char host[48];
  snprintf(host, sizeof(host), "%s-%s", deviceType, deviceId);
  if (!MDNS.begin(host)) return;
  MDNS.addService(Config::SERVICE, "tcp", Config::PORT);
  MDNS.addServiceTxt(Config::SERVICE, "tcp", "id", deviceId);
  MDNS.addServiceTxt(Config::SERVICE, "tcp", "type", deviceType);
  MDNS.addServiceTxt(Config::SERVICE, "tcp", "name", deviceName);
  MDNS.addServiceTxt(Config::SERVICE, "tcp", "fw", deviceFirmware);
  MDNS.addServiceTxt(Config::SERVICE, "tcp", "proto", String(Config::PROTOCOL).c_str());
  announced = true;
  Serial.printf("[DeviceControl] %s.local (%s)\n", host, WiFi.localIP().toString().c_str());
}

void reconnect() {
  uint32_t now = millis();
  if (now - lastRetry < Config::WIFI_RETRY_MS) return;
  lastRetry = now;
  WiFi.begin();
}

}

void begin(const char* name, const char* type, const char* id, const char* firmware) {
  deviceName = name;
  deviceType = type;
  deviceId = id;
  deviceFirmware = firmware;
  WiFi.setAutoReconnect(true);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    reconnect();
    return;
  }
  if (!announced) announce();
#if defined(ESP8266)
  MDNS.update();
#endif
}

}
}
