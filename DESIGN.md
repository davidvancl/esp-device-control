# esp-device-control – design

Control all ESP projects from one Android app over the local WiFi network. This document is the shared contract between the ESP library (`esp-device-control`) and the Android app. Both sides must follow it; any change to the protocol is made here first.

Status: ESP library 0.1.0 implemented (protocol v1 without HMAC), integrated into project-ikarus; Android app 0.1 implemented in `esp-control-app` (builds, protocol unit tests pass, not yet tested against hardware).

## Goals

- One Android app controls every ESP project, current and future.
- The app finds devices on the network automatically, pairs with them and controls them.
- The ESP library only handles communication with the app (discovery, connection, pairing, passing values). It knows nothing about any project: a project only exposes named values that can be read (`get`) and / or written (`set`).
- The app knows the device types: the UI for each `type` is defined in the app, based on the value list in [Device types](#device-types).
- The protocol is independent of the transport. Version 1 uses WiFi (WebSocket). BLE for ESP32 devices or MQTT for remote access can be added later as other transports carrying the same messages.

## Devices

| Project | Board | Notes |
|---|---|---|
| project-ikarus | ESP8266 (nodemcuv2) | **pilot**: control panel – 2x 2-digit + 2x 4-digit 7-segment displays (MAX7219), buttons and key switch (PCF8574A), green and red LED, Slack / GitLab integration in progress |
| coin-jar | ESP32 (esp32dev) | coin counter (IR light barrier), 5 RGB LEDs, I2S sound |
| infinity-cube-lamp | ESP8266 (nodemcuv2) | RGB strip, IR remote, two modes |
| gitlab-pipeline-semaphor | ESP8266 (nodemcuv2) | relays, TFT, HTTPS to GitLab – tight on RAM, migrate last |

All devices already use [esp-ota-updater](https://github.com/davidvancl/esp-ota-updater) (`OtaUpdater::run()` connects WiFi at boot and leaves it connected; `saveSecret`/`loadSecret` store values persistently).

## Architecture

```
 Android app                                   ESP device
┌─────────────────────┐                     ┌──────────────────────────┐
│ UI per device type  │                     │ project: get / set by id │
├─────────────────────┤                     ├──────────────────────────┤
│ Protocol (JSON)     │ ◄── same messages ─►│ DeviceControl (protocol) │
├─────────────────────┤                     ├──────────────────────────┤
│ WsConnection        │ ◄──── WebSocket ───►│ AsyncWebServer  /ws      │
│ NsdManager (mDNS)   │ ◄──── mDNS ─────────│ _espctl._tcp             │
└─────────────────────┘                     └──────────────────────────┘
```

## Discovery

The device advertises an mDNS / DNS-SD service:

- Service type: `_espctl._tcp`, port `80`
- Hostname: `<type>-<id>.local`, e.g. `ikarus-a1b2c3.local`
- TXT records:

| Key | Example | Meaning |
|---|---|---|
| `id` | `a1b2c3` | chip ID in lowercase hex, stable across reboots, unique device identity |
| `type` | `ikarus` | device type, selects the UI in the app |
| `name` | `Ikarus` | human readable name |
| `fw` | `0.0.3` | firmware version (`FW_VERSION`) |
| `proto` | `1` | protocol version |

Fallbacks in the app, because mDNS is unreliable on some routers and phones:

1. Remember the last IP of every known device and try it first.
2. Allow entering an IP address manually.
3. Later: a UDP broadcast "who is there" discovery.

Devices are identified by `id`, never by IP.

## Transport

- One WebSocket per app connection at `ws://<host>/ws`. Each text frame carries exactly one JSON object; frames larger than 512 bytes and fragmented frames are ignored.
- The device accepts at most 3 clients at a time. A connection that is not authenticated is closed after 10 s, unless pairing is in progress on it.
- Keepalive with WebSocket ping/pong every 15 s on both sides.

## Protocol v1

Every message is a JSON object with a type field `t`. Requests from the app carry `seq` (an integer chosen by the app, incremented per request); the response to a request carries the same `seq`. Unsolicited messages from the device have no `seq`.

A device exposes **values** identified by `id`. A value can be readable (`get`), writable (`set`) or both. Values are plain JSON (bool, number, string, array, object); their meaning is defined per device type.

### App → device

| `t` | Fields | Reply |
|---|---|---|
| `hello` | `proto`, `token` (optional) | `info` |
| `get` | – | `state` with all readable values |
| `set` | `id`, `v` (any JSON, may be omitted for actions) | `ack` or `err`; the change is also broadcast as `event` |
| `pair` | – | `ack`, then `pairing` / `paired` messages |
| `pin` | `pin` (string) | `ack` and `paired`, or `err` `auth`; only during pairing with `method` `pin` |
| `unpair` | – | `ack`; the token used by this connection is removed |
| `restart` | – | `ack`; the device restarts about 0.5 s later, all connections drop and the app reconnects |

```json
{"t":"hello","seq":1,"proto":1,"token":"9f2c...e1"}
{"t":"get","seq":2}
{"t":"set","seq":3,"id":"green","v":40}
{"t":"pair","seq":4}
{"t":"pin","seq":5,"pin":"4821"}
{"t":"restart","seq":6}
```

### Device → app

| `t` | Fields | When |
|---|---|---|
| `info` | `proto`, `id`, `name`, `type`, `fw`, `authed`, `get` and `set` (arrays of ids, only when `authed` is `true`) | reply to `hello` |
| `state` | `values` (object `id → value`) | reply to `get` |
| `ack` | – | request succeeded |
| `err` | `code`, `msg` | request failed |
| `event` | `values` (object `id → value`) | a value changed or something happened on the device (e.g. button pressed); sent to all **authenticated** clients |
| `pairing` | `state` (`waiting`, `timeout`, `rejected`), `method` (`pin` or `confirm`, with `waiting`), `timeout` (seconds, with `waiting`) | pairing progress |
| `paired` | `token` | pairing succeeded |

```json
{"t":"info","seq":1,"proto":1,"id":"a1b2c3","name":"Ikarus","type":"ikarus","fw":"0.0.3","authed":true,
 "get":["key"],
 "set":["left2","right2","left4","right4","intensity","green","red"]}
{"t":"state","seq":2,"values":{"key":false}}
{"t":"ack","seq":3}
{"t":"err","seq":3,"code":"invalid","msg":"value rejected"}
{"t":"event","values":{"key":true}}
{"t":"event","values":{"greenButton":true}}
```

- After a successful `set` the device broadcasts an `event`: with the value read back from the getter if the id is readable, otherwise with the value that was set. Other connected apps stay in sync; the device stores nothing.
- Write-only values (e.g. displays, PWM LEDs – hardware that cannot be read back) never appear in `state`. The app shows the last value it sent or saw in an `event` during the current connection and clears them on reconnect, because the device may have rebooted.
- The app must ignore unknown fields and unknown ids, so newer firmware keeps working with older apps.

### Error codes

| `code` | Meaning |
|---|---|
| `bad_request` | invalid JSON, missing field, unknown message type |
| `unknown_id` | no value with this `id` |
| `invalid` | the device rejected the value (wrong type or out of range) |
| `readonly` | the value cannot be set |
| `auth` | not paired / invalid token / wrong PIN |
| `proto` | unsupported protocol version |
| `busy` | pairing already in progress |
| `unsupported` | the device does not support the request (e.g. `pair` without a pairing method) |

### Versioning

- `proto` is an integer. The device replies with `err` code `proto` if it does not support the version sent in `hello`.
- Adding optional fields or new ids does not change `proto`. Changing the meaning of existing messages does.

## Pairing and security

- Nothing can be read or controlled without a valid token. After `hello` without a valid token the device answers only its identity – the same data it already publishes over mDNS:

  ```json
  {"t":"info","seq":1,"proto":1,"id":"a1b2c3","name":"Ikarus","type":"ikarus","fw":"0.0.3","authed":false}
  ```

  Only `hello`, `pair` and `pin` are allowed; everything else returns `err` `auth`, and the connection receives no `event` messages.
- Pairing flow:
  1. App sends `pair`.
  2. Device replies `ack` and `{"t":"pairing","state":"waiting","method":"pin","timeout":60}` (or `"method":"confirm"`), and starts pairing on its hardware.
  3. Depending on `method`:
     - `pin` – devices with a display. The device shows a random 4-digit PIN (ikarus: on the first 4-digit display). The app asks the user for it and sends `{"t":"pin","pin":"4821"}`. Three wrong attempts end pairing with `rejected`.
     - `confirm` – devices without a display. The user confirms on the device (e.g. a button). The app only shows "confirm on the device".
  4. Device generates a random token (32 hex chars), stores it and sends `{"t":"paired","token":"..."}`. Otherwise `pairing` with `timeout` or `rejected`.
- The device keeps up to 4 tokens (one per phone); pairing a fifth replaces the oldest and disconnects the phone that used it.
- The app stores the token per device `id` and sends it in every `hello`.
- Traffic in the LAN is not encrypted (TLS is too heavy for ESP8266). Planned v2: challenge–response – the device sends a `nonce` in `info`, the app answers with `HMAC-SHA256(token, nonce)`, so the token never goes over the network.

## Device types

The app implements one screen per `type`. Values not listed here are ignored by the app.

### `ikarus`

| id | Access | Value | Meaning |
|---|---|---|---|
| `left2` | set | int 0–99 or `null` | first 2-digit display; `null` blanks it |
| `right2` | set | int 0–99 or `null` | second 2-digit display; `null` blanks it |
| `left4` | set | int 0–9999 or `null` | first 4-digit display; `null` blanks it |
| `right4` | set | int 0–9999 or `null` | second 4-digit display; `null` blanks it |
| `intensity` | get, set | int 0–15 | brightness of all displays (8 after boot) |
| `green` | get, set | int 0–100 (%) | green button LED (100 after boot) |
| `red` | get, set | int 0–100 (%) | red button LED (100 after boot) |
| `key` | get | bool | key switch is on; also sent as `event` when it changes |
| `greenButton` | event | `true` | green button pressed |
| `redButton` | event | `true` | red button pressed |
| `button1`, `button2`, `button3` | event | `true` | buttons 1–3 pressed |

Pairing method: `pin`. Displays are cleared when pairing ends.

## ESP library

Repository: `esp-device-control`, same style as esp-ota-updater (namespace API, ESP8266 + ESP32, used via a git tag in `lib_deps`).

Dependencies:

- `ESP32Async/ESPAsyncWebServer` (+ `ESP32Async/ESPAsyncTCP` on ESP8266, `AsyncTCP` on ESP32)
- `bblanchon/ArduinoJson` v7
- `ESP8266mDNS` / `ESPmDNS` (part of the cores)
- esp-ota-updater (token storage via `saveSecret`)

API ([src/DeviceControl.h](src/DeviceControl.h)):

```cpp
DeviceControl::begin("Ikarus", "ikarus");

DeviceControl::get<bool>("key", Panel::keyOn);
DeviceControl::set<int>("green", Panel::setGreen);
DeviceControl::set("left4", [](JsonVariantConst v) {
  if (!v.is<int>()) return false;
  Display::showFourDigits(1, 0, v.as<int>());
  return true;
});
DeviceControl::pairWithPin(Display::showPin, Display::clear);

DeviceControl::loop();

DeviceControl::notify("key");
DeviceControl::notify("greenButton", true);
```

- `get(id, Getter)` / `set(id, Setter)` take JSON callbacks; a setter returns `false` to reject the value (`err` `invalid`). `get<T>` / `set<T>` are typed shortcuts.
- `notify(id)` sends the current getter value as `event`, `notify(id, value)` sends any value.
- Pairing: `pairWithPin(show, hide)` for devices with a display, or `pairWithConfirm(start, stop)` plus `confirmPairing()` called by the project when the user confirms on the device.
- Ids are stored as pointers – pass string literals.

Source layout:

| File | Responsibility |
|---|---|
| `DeviceControl.h/.cpp` | public API, main loop, connection lifecycle |
| `dc/Transport` | AsyncWebServer + WebSocket, queue of incoming events |
| `dc/Discovery` | mDNS announcement, WiFi reconnect |
| `dc/Protocol` | message handling, replies, events |
| `dc/Sessions` | connected clients and their auth state |
| `dc/Tokens` | token storage and validation |
| `dc/Pairing` | pairing state machine (PIN / confirm) |
| `dc/Registry` | registered getters and setters |
| `dc/System` | device-level actions (restart) |
| `dc/Platform` | ESP8266 / ESP32 differences (lock, random, chip id) |
| `dc/Config` | limits and timeouts |

Implementation rules:

- Server callbacks run outside the main loop (lwIP / async task). Incoming events are only queued there and processed in `DeviceControl::loop()`, so getters and setters always run in the main loop.
- If WiFi was not available at boot, the library retries `WiFi.begin()` every 60 s and starts mDNS once connected; the WebSocket server listens from `begin()`.
- Tokens are stored with `OtaUpdater::saveSecret` under the key `dc_tokens` (up to 4 × 32 hex chars).
- Flash on coin-jar (ESP32): the example build uses 82 % of the default 1.25 MB OTA slot; if a project does not fit, switch to `board_build.partitions = min_spiffs.csv` (1.9 MB slots, OTA keeps working).
- RAM on ESP8266: measure `ESP.getFreeHeap()` on every project, especially together with HTTPS calls.

## Android app

Stack:

- Kotlin, Jetpack Compose, ViewModel, Coroutines / Flow
- `NsdManager` for mDNS discovery
- OkHttp for WebSocket (`pingInterval(15, SECONDS)`)
- kotlinx.serialization for JSON
- DataStore for known devices (`id`, name, type, last IP, token)

Structure:

- `DeviceRepository` – merges devices found by mDNS with stored ones, tracks online / offline.
- `DeviceConnection` (interface: `send(message)`, `incoming: Flow<Message>`, connection state) with `WsConnection` as the v1 implementation. A BLE implementation can be added later without touching UI or logic.
- Protocol model – sealed classes for the messages above; unknown fields ignored (`ignoreUnknownKeys = true`).
- `DeviceState` – current values per device: from `state`, `event` and values the app has set.
- One screen per device type (see [Device types](#device-types)); unknown types show a generic list of `get` values.

Screens:

1. Device list (online / offline, type icon, firmware version)
2. Pairing
3. Device detail – the screen for its type
4. Settings (manual IP, remove device)

Gotchas:

- Android 9+ blocks cleartext `http://` and `ws://` by default: add a `network_security_config` that permits cleartext traffic (local devices only).
- Close the WebSocket when the app goes to background, reconnect with backoff when it returns.
- Debounce slider changes (about 100 ms) so the device is not flooded with `set` messages; apply incoming `event` values to the UI except for the control the user is currently dragging.
- Requests time out after 3 s without a reply.

## Roadmap

1. ~~Protocol and ESP library 0.1~~ – done.
2. ~~Integrate into project-ikarus~~ – done; test on hardware with `websocat`, measure free heap together with the HTTPS calls to Slack / GitLab.
3. Create the GitHub repository, tag `v0.1.0`, switch ikarus `lib_deps` from `symlink://` to the git URL.
4. ~~Android app 0.1: discovery, pairing, ikarus screen~~ – implemented; test on a phone against ikarus.
5. HMAC challenge–response.
6. Roll out to coin-jar, infinity-cube-lamp, then gitlab-pipeline-semaphor (add their tables to [Device types](#device-types)).
7. Later: WiFi provisioning from the app (fallback access point), BLE transport for ESP32, remote access via MQTT.

## Using this document with Claude Code in the Android project

Put a `CLAUDE.md` into the root of the Android project with:

```markdown
This is the Android app for controlling ESP devices (esp-device-control).
The protocol, device types and architecture are defined in
C:\Users\vancl\Documents\workspace\esp-device-control\DESIGN.md – read it before working on
networking, discovery, the protocol model or device screens. Do not change the protocol
on the app side only; protocol changes are made in DESIGN.md first.
```
