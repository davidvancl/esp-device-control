# esp-device-control

Communication library for ESP8266 and ESP32 that lets the Android app find the device on the local WiFi network, pair with it and read or write its values. The library knows nothing about the project: the project exposes named values with `get` / `set` callbacks.

The protocol, security model, device types and the plan for the Android app are in [DESIGN.md](DESIGN.md).

## Features

- Discovery over mDNS (`_espctl._tcp`)
- WebSocket at `ws://<device>/ws` with JSON messages (protocol v1)
- Values by id: readable, writable or both; events pushed to connected apps
- Pairing with a PIN shown on the device, or confirmed on the device; nothing can be read or controlled without a token
- Remote restart of the device from the app
- Binary data streams from the app (e.g. audio)

## Installation

```ini
lib_deps =
    https://github.com/davidvancl/esp-ota-updater.git#v1.1.1
    https://github.com/davidvancl/esp-device-control.git#v0.4.0
```

The project must define `FW_VERSION` and `OTA_REPO` build flags (required by esp-ota-updater, which stores the pairing tokens).

## Usage

```cpp
#include <DeviceControl.h>
#include <OtaUpdater.h>

void setup() {
  OtaUpdater::run(WIFI_CREDENTIALS);

  DeviceControl::begin("Ikarus", "ikarus");
  DeviceControl::get<bool>("key", Panel::keyOn);
  DeviceControl::set<int>("green", Panel::setGreen);
  DeviceControl::set("left4", [](JsonVariantConst v) {
    if (!v.is<int>()) return false;
    Display::showFourDigits(1, 0, v.as<int>());
    return true;
  });
  DeviceControl::pairWithPin(Display::showPin, Display::clear);
}

void loop() {
  DeviceControl::loop();
}
```

| Function | Purpose |
|---|---|
| `begin(name, type)` | start server and discovery |
| `loop()` | process messages, call from `loop()` |
| `get(id, getter)` / `get<T>(id, fn)` | expose a readable value |
| `set(id, setter)` / `set<T>(id, fn)` | expose a writable value; a JSON setter returns `false` to reject the value |
| `notify(id)` | send the current value of `id` to connected apps |
| `notify(id, value)` | send any value, e.g. `notify("greenButton", true)` |
| `onData(handler)` | receive binary frames from paired apps, e.g. an audio stream; the handler runs in the network task, so it must be fast and thread-safe (copy into a buffer) |
| `pairWithPin(show, hide)` | pairing with a 4-digit PIN shown on the device |
| `pairWithPin(show, hide, {symbols...})` | PIN made of 2–10 symbols, e.g. colors `"#FF0000"`; `show` gets symbol indices as digits |
| `pairWithConfirm(start, stop)` + `confirmPairing()` | pairing confirmed on the device |

Getters and setters always run in the main loop, the `onData` handler runs in the network task. Ids are stored as pointers, pass string literals.

A complete example is in [examples/basic](examples/basic/main.cpp).

## Testing without the app

With [websocat](https://github.com/vi/websocat):

```
websocat ws://ikarus-a1b2c3.local/ws
{"t":"hello","seq":1,"proto":1}
{"t":"pair","seq":2}
{"t":"pin","seq":3,"pin":"4821"}
{"t":"get","seq":4}
{"t":"set","seq":5,"id":"green","v":40}
{"t":"restart","seq":6}
```

After `paired` the connection is authorized; on the next connection send the received token in `hello`. An unauthenticated connection is closed after 10 s unless pairing is in progress.
