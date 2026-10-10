#include "Protocol.h"

#include "Config.h"
#include "Pairing.h"
#include "Tokens.h"
#include "Transport.h"

namespace DeviceControl {
namespace Protocol {

namespace {

const char* deviceName = "";
const char* deviceType = "";
const char* deviceId = "";
const char* deviceFirmware = "";

struct Handler {
  const char* type;
  bool needsAuth;
  void (*run)(Session&, JsonObjectConst);
};

const char* errorMessage(const char* code) {
  if (!strcmp(code, "bad_request")) return "invalid request";
  if (!strcmp(code, "unknown_id")) return "unknown id";
  if (!strcmp(code, "invalid")) return "value rejected";
  if (!strcmp(code, "readonly")) return "value cannot be set";
  if (!strcmp(code, "auth")) return "not authorized";
  if (!strcmp(code, "proto")) return "unsupported protocol version";
  if (!strcmp(code, "busy")) return "pairing already in progress";
  if (!strcmp(code, "unsupported")) return "not supported by this device";
  return code;
}

void sendTo(uint32_t client, const JsonDocument& doc) {
  String text;
  serializeJson(doc, text);
  Transport::send(client, text);
}

void broadcast(const JsonDocument& doc) {
  String text;
  serializeJson(doc, text);
  for (const Session& session : Sessions::all()) {
    if (session.used && session.authed) Transport::send(session.client, text);
  }
}

JsonDocument reply(const char* type, JsonVariantConst seq) {
  JsonDocument doc;
  doc["t"] = type;
  if (!seq.isNull()) doc["seq"] = seq;
  return doc;
}

void ack(Session& session, JsonVariantConst seq) {
  sendTo(session.client, reply("ack", seq));
}

void fail(Session& session, JsonVariantConst seq, const char* code) {
  JsonDocument doc = reply("err", seq);
  doc["code"] = code;
  doc["msg"] = errorMessage(code);
  sendTo(session.client, doc);
}

void revokeOthers(const char* token, uint32_t except) {
  for (Session& session : Sessions::all()) {
    if (!session.used || !session.authed || session.client == except) continue;
    if (strcmp(session.token, token) != 0) continue;
    session.revoke();
    Transport::close(session.client);
  }
}

void hello(Session& session, JsonObjectConst msg) {
  JsonVariantConst seq = msg["seq"];
  if (!msg["proto"].is<int>() || msg["proto"].as<int>() != Config::PROTOCOL) {
    fail(session, seq, "proto");
    return;
  }
  const char* token = msg["token"].as<const char*>();
  if (Tokens::isValid(token)) session.authorize(token);

  JsonDocument doc = reply("info", seq);
  doc["proto"] = Config::PROTOCOL;
  doc["id"] = deviceId;
  doc["name"] = deviceName;
  doc["type"] = deviceType;
  doc["fw"] = deviceFirmware;
  doc["authed"] = session.authed;
  if (session.authed) {
    JsonArray readable = doc["get"].to<JsonArray>();
    JsonArray writable = doc["set"].to<JsonArray>();
    for (const Registry::Entry& entry : Registry::all()) {
      if (entry.getter) readable.add(entry.id);
      if (entry.setter) writable.add(entry.id);
    }
  }
  sendTo(session.client, doc);
}

void getValues(Session& session, JsonObjectConst msg) {
  JsonDocument doc = reply("state", msg["seq"]);
  JsonObject values = doc["values"].to<JsonObject>();
  for (const Registry::Entry& entry : Registry::all()) {
    if (entry.getter) entry.getter(values[entry.id].to<JsonVariant>());
  }
  sendTo(session.client, doc);
}

void setValue(Session& session, JsonObjectConst msg) {
  JsonVariantConst seq = msg["seq"];
  Registry::Entry* entry = Registry::find(msg["id"].as<const char*>());
  if (!entry) {
    fail(session, seq, "unknown_id");
    return;
  }
  if (!entry->setter) {
    fail(session, seq, "readonly");
    return;
  }
  JsonVariantConst value = msg["v"];
  if (!entry->setter(value)) {
    fail(session, seq, "invalid");
    return;
  }
  ack(session, seq);
  if (entry->getter) {
    publish(*entry);
  } else {
    publish(entry->id, value);
  }
}

void pair(Session& session, JsonObjectConst msg) {
  JsonVariantConst seq = msg["seq"];
  if (Pairing::method() == Pairing::Method::None) {
    fail(session, seq, "unsupported");
    return;
  }
  if (Pairing::active()) {
    fail(session, seq, "busy");
    return;
  }
  ack(session, seq);

  JsonDocument doc;
  doc["t"] = "pairing";
  doc["state"] = "waiting";
  doc["method"] = Pairing::methodName();
  doc["timeout"] = Config::PAIRING_TIMEOUT_MS / 1000;
  sendTo(session.client, doc);

  Pairing::start(session.client);
}

void pin(Session& session, JsonObjectConst msg) {
  JsonVariantConst seq = msg["seq"];
  switch (Pairing::checkPin(session.client, msg["pin"].as<const char*>())) {
    case Pairing::PinResult::Invalid:
      fail(session, seq, "bad_request");
      break;
    case Pairing::PinResult::Accepted:
      ack(session, seq);
      completePairing(session);
      break;
    case Pairing::PinResult::Wrong:
      fail(session, seq, "auth");
      break;
    case Pairing::PinResult::Rejected:
      fail(session, seq, "auth");
      sendPairingState(session.client, "rejected");
      Pairing::end();
      break;
  }
}

void unpair(Session& session, JsonObjectConst msg) {
  ack(session, msg["seq"]);
  char token[Config::TOKEN_LENGTH + 1];
  memcpy(token, session.token, sizeof(token));
  session.revoke();
  Tokens::remove(token);
  revokeOthers(token, session.client);
}

const Handler HANDLERS[] = {
  {"hello", false, hello},
  {"pair", false, pair},
  {"pin", false, pin},
  {"get", true, getValues},
  {"set", true, setValue},
  {"unpair", true, unpair},
};

}

void begin(const char* name, const char* type, const char* id, const char* firmware) {
  deviceName = name;
  deviceType = type;
  deviceId = id;
  deviceFirmware = firmware;
}

void handle(Session& session, const String& text) {
  JsonDocument doc;
  if (deserializeJson(doc, text) || !doc.is<JsonObject>()) {
    fail(session, JsonVariantConst(), "bad_request");
    return;
  }
  JsonObjectConst msg = doc.as<JsonObjectConst>();
  const char* type = msg["t"].as<const char*>();
  for (const Handler& handler : HANDLERS) {
    if (!type || strcmp(type, handler.type) != 0) continue;
    if (handler.needsAuth && !session.authed) {
      fail(session, msg["seq"], "auth");
      return;
    }
    handler.run(session, msg);
    return;
  }
  fail(session, msg["seq"], "bad_request");
}

void publish(const Registry::Entry& entry) {
  if (!entry.getter || !Sessions::anyAuthed()) return;
  JsonDocument doc;
  doc["t"] = "event";
  JsonObject values = doc["values"].to<JsonObject>();
  entry.getter(values[entry.id].to<JsonVariant>());
  broadcast(doc);
}

void publish(const char* id, JsonVariantConst value) {
  if (!Sessions::anyAuthed()) return;
  JsonDocument doc;
  doc["t"] = "event";
  doc["values"][id] = value;
  broadcast(doc);
}

void sendPairingState(uint32_t client, const char* state) {
  JsonDocument doc;
  doc["t"] = "pairing";
  doc["state"] = state;
  sendTo(client, doc);
}

void completePairing(Session& session) {
  char token[Config::TOKEN_LENGTH + 1];
  char dropped[Config::TOKEN_LENGTH + 1];
  Tokens::issue(token, dropped);
  if (dropped[0]) revokeOthers(dropped, session.client);
  session.authorize(token);
  Pairing::end();

  JsonDocument doc;
  doc["t"] = "paired";
  doc["token"] = token;
  sendTo(session.client, doc);
}

}
}
