#include "Platform.h"

namespace DeviceControl {

#if defined(ESP32)
static SemaphoreHandle_t mutex = nullptr;

Lock::Lock() {
  if (!mutex) mutex = xSemaphoreCreateMutex();
  xSemaphoreTake(mutex, portMAX_DELAY);
}

Lock::~Lock() {
  xSemaphoreGive(mutex);
}

uint32_t randomWord() {
  return esp_random();
}
#else
Lock::Lock() {}

Lock::~Lock() {}

uint32_t randomWord() {
  return RANDOM_REG32;
}
#endif

const char* chipId() {
  static char id[7] = {};
  if (id[0]) return id;
#if defined(ESP8266)
  uint32_t chip = ESP.getChipId();
#else
  uint32_t chip = static_cast<uint32_t>(ESP.getEfuseMac() >> 24);
#endif
  snprintf(id, sizeof(id), "%06x", static_cast<unsigned>(chip & 0xFFFFFF));
  return id;
}

}
