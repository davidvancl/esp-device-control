#include "Pairing.h"

#include "Config.h"
#include "Platform.h"

namespace DeviceControl {
namespace Pairing {

namespace {

Method current = Method::None;
std::function<void(const char*)> showPin;
std::function<void()> hidePin;
std::function<void()> startConfirm;
std::function<void()> stopConfirm;

bool running = false;
uint32_t client = 0;
uint32_t startedAt = 0;
uint8_t attempts = 0;
char pin[Config::PIN_LENGTH + 1] = {};

}

void usePin(std::function<void(const char*)> show, std::function<void()> hide) {
  current = Method::Pin;
  showPin = show;
  hidePin = hide;
}

void useConfirm(std::function<void()> start, std::function<void()> stop) {
  current = Method::Confirm;
  startConfirm = start;
  stopConfirm = stop;
}

Method method() {
  return current;
}

const char* methodName() {
  return current == Method::Pin ? "pin" : "confirm";
}

bool active() {
  return running;
}

bool isOwner(uint32_t id) {
  return running && client == id;
}

uint32_t owner() {
  return client;
}

void start(uint32_t id) {
  running = true;
  client = id;
  startedAt = millis();
  attempts = 0;
  if (current == Method::Pin) {
    snprintf(pin, sizeof(pin), "%04u", static_cast<unsigned>(randomWord() % 10000));
    if (showPin) showPin(pin);
  } else if (startConfirm) {
    startConfirm();
  }
}

PinResult checkPin(uint32_t id, const char* value) {
  if (!isOwner(id) || current != Method::Pin) return PinResult::Invalid;
  if (value && !strcmp(value, pin)) return PinResult::Accepted;
  return ++attempts >= Config::PIN_ATTEMPTS ? PinResult::Rejected : PinResult::Wrong;
}

bool canConfirm() {
  return running && current == Method::Confirm;
}

bool expired() {
  return running && millis() - startedAt >= Config::PAIRING_TIMEOUT_MS;
}

void end() {
  if (!running) return;
  running = false;
  memset(pin, 0, sizeof(pin));
  if (current == Method::Pin && hidePin) hidePin();
  if (current == Method::Confirm && stopConfirm) stopConfirm();
}

}
}
