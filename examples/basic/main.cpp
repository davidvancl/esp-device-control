#include <Arduino.h>
#include <DeviceControl.h>
#include <OtaUpdater.h>

const int LED_PIN = 2;
const int BOOT_PIN = 0;

bool ledOn = false;
bool bootPressed = false;

void setLed(bool on) {
  ledOn = on;
  digitalWrite(LED_PIN, on ? HIGH : LOW);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BOOT_PIN, INPUT_PULLUP);

  OtaUpdater::run();

  DeviceControl::begin("Example", "example");
  DeviceControl::get<bool>("led", [] { return ledOn; });
  DeviceControl::set<bool>("led", setLed);
  DeviceControl::get<unsigned long>("uptime", [] { return millis() / 1000; });
  DeviceControl::set("hello", [](JsonVariantConst) {
    Serial.println("hello");
    return true;
  });
  DeviceControl::pairWithConfirm([] { Serial.println("Press BOOT to confirm pairing"); }, nullptr);
}

void loop() {
  DeviceControl::loop();

  bool pressed = digitalRead(BOOT_PIN) == LOW;
  if (pressed && !bootPressed) {
    DeviceControl::notify("boot", true);
    DeviceControl::confirmPairing();
  }
  bootPressed = pressed;
}
