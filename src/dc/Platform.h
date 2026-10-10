#pragma once

#include <Arduino.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#if __has_include(<esp_random.h>)
#include <esp_random.h>
#endif
#else
#error "DeviceControl supports only ESP8266 and ESP32"
#endif

namespace DeviceControl {

class Lock {
 public:
  Lock();
  ~Lock();
};

uint32_t randomWord();

const char* chipId();

}
