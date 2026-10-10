#pragma once

#include <ArduinoJson.h>

#include "Registry.h"
#include "Sessions.h"

namespace DeviceControl {
namespace Protocol {

void begin(const char* name, const char* type, const char* id, const char* firmware);

void handle(Session& session, const String& text);

void publish(const Registry::Entry& entry);

void publish(const char* id, JsonVariantConst value);

void sendPairingState(uint32_t client, const char* state);

void completePairing(Session& session);

}
}
