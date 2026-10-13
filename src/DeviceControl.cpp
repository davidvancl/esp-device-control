#include "DeviceControl.h"

#include "dc/Config.h"
#include "dc/Discovery.h"
#include "dc/Pairing.h"
#include "dc/Platform.h"
#include "dc/Protocol.h"
#include "dc/Registry.h"
#include "dc/Sessions.h"
#include "dc/System.h"
#include "dc/Tokens.h"
#include "dc/Transport.h"

namespace DeviceControl {

namespace {

#ifdef FW_VERSION
const char* const FIRMWARE = FW_VERSION;
#else
const char* const FIRMWARE = "";
#endif

bool started = false;
uint32_t lastCleanup = 0;

void connect(uint32_t client) {
  if (!Sessions::open(client)) Transport::close(client);
}

void disconnect(uint32_t client) {
  if (Pairing::isOwner(client)) Pairing::end();
  Sessions::release(client);
}

void receive(uint32_t client, const String& text) {
  Session* session = Sessions::find(client);
  if (session) Protocol::handle(*session, text);
}

void processIncoming() {
  Transport::Incoming incoming;
  while (Transport::poll(incoming)) {
    switch (incoming.kind) {
      case Transport::Kind::Connect:
        connect(incoming.client);
        break;
      case Transport::Kind::Disconnect:
        disconnect(incoming.client);
        break;
      case Transport::Kind::Message:
        receive(incoming.client, incoming.data);
        break;
    }
  }
}

void expirePairing() {
  if (!Pairing::expired()) return;
  Protocol::sendPairingState(Pairing::owner(), "timeout");
  Pairing::end();
}

void dropUnauthorized() {
  uint32_t now = millis();
  for (Session& session : Sessions::all()) {
    if (!session.used || session.authed || Pairing::isOwner(session.client)) continue;
    if (now - session.since < Config::AUTH_TIMEOUT_MS) continue;
    session.since = now;
    Transport::close(session.client);
  }
}

void cleanup() {
  uint32_t now = millis();
  if (now - lastCleanup < Config::CLEANUP_MS) return;
  lastCleanup = now;
  Transport::cleanup();
  for (Session& session : Sessions::all()) {
    if (session.used && !Transport::isConnected(session.client)) disconnect(session.client);
  }
}

void shareAuthorized() {
  uint32_t clients[Config::MAX_CLIENTS] = {};
  size_t length = 0;
  for (const Session& session : Sessions::all()) {
    if (session.used && session.authed) clients[length++] = session.client;
  }
  Transport::setAuthorized(clients, length);
}

}

void begin(const char* name, const char* type) {
  if (started) return;
  started = true;
  Tokens::load();
  Protocol::begin(name, type, chipId(), FIRMWARE);
  Discovery::begin(name, type, chipId(), FIRMWARE);
  Transport::begin();
}

void loop() {
  if (!started) return;
  Discovery::loop();
  processIncoming();
  expirePairing();
  dropUnauthorized();
  cleanup();
  shareAuthorized();
  System::loop();
}

void get(const char* id, Getter getter) {
  Registry::entry(id).getter = getter;
}

void set(const char* id, Setter setter) {
  Registry::entry(id).setter = setter;
}

void notify(const char* id) {
  Registry::Entry* entry = Registry::find(id);
  if (entry) Protocol::publish(*entry);
}

void notify(const char* id, JsonVariantConst value) {
  Protocol::publish(id, value);
}

void pairWithPin(std::function<void(const char* pin)> show, std::function<void()> hide,
                 std::initializer_list<const char*> symbols) {
  Pairing::usePin(show, hide, std::vector<const char*>(symbols));
}

void pairWithConfirm(std::function<void()> start, std::function<void()> stop) {
  Pairing::useConfirm(start, stop);
}

void onData(std::function<void(const uint8_t* data, size_t length)> handler) {
  Transport::onData(handler);
}

void confirmPairing() {
  if (!Pairing::canConfirm()) return;
  Session* session = Sessions::find(Pairing::owner());
  if (!session) {
    Pairing::end();
    return;
  }
  Protocol::completePairing(*session);
}

}
