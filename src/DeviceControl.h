#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>

namespace DeviceControl {

using Getter = std::function<void(JsonVariant)>;
using Setter = std::function<bool(JsonVariantConst)>;

void begin(const char* name, const char* type);

void loop();

void get(const char* id, Getter getter);

void set(const char* id, Setter setter);

void notify(const char* id);

void notify(const char* id, JsonVariantConst value);

void pairWithPin(std::function<void(const char* pin)> show, std::function<void()> hide);

void pairWithConfirm(std::function<void()> start, std::function<void()> stop);

void confirmPairing();

template <typename T>
void get(const char* id, std::function<T()> getter) {
  get(id, Getter([getter](JsonVariant out) { out.set(getter()); }));
}

template <typename T>
void set(const char* id, std::function<void(T)> setter) {
  set(id, Setter([setter](JsonVariantConst value) {
    if (!value.is<T>()) return false;
    setter(value.as<T>());
    return true;
  }));
}

template <typename T>
void notify(const char* id, T value) {
  JsonDocument doc;
  doc.set(value);
  notify(id, doc.as<JsonVariantConst>());
}

}
