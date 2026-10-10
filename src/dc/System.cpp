#include "System.h"

#include "Config.h"
#include "Platform.h"

namespace DeviceControl {
namespace System {

namespace {
bool restartPending = false;
uint32_t restartRequestedAt = 0;
}

void requestRestart() {
  restartPending = true;
  restartRequestedAt = millis();
}

void loop() {
  if (!restartPending || millis() - restartRequestedAt < Config::RESTART_DELAY_MS) return;
  Serial.println("[DeviceControl] restart requested by app");
  ESP.restart();
}

}
}
