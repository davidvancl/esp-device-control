#include "Sessions.h"

namespace DeviceControl {

void Session::authorize(const char* value) {
  authed = true;
  strncpy(token, value, Config::TOKEN_LENGTH);
  token[Config::TOKEN_LENGTH] = '\0';
}

void Session::revoke() {
  authed = false;
  token[0] = '\0';
  since = millis();
}

namespace Sessions {

namespace {
Session sessions[Config::MAX_CLIENTS];
}

Session* open(uint32_t client) {
  for (Session& session : sessions) {
    if (session.used) continue;
    session = Session();
    session.used = true;
    session.client = client;
    session.since = millis();
    return &session;
  }
  return nullptr;
}

Session* find(uint32_t client) {
  for (Session& session : sessions) {
    if (session.used && session.client == client) return &session;
  }
  return nullptr;
}

void release(uint32_t client) {
  Session* session = find(client);
  if (session) *session = Session();
}

Session (&all())[Config::MAX_CLIENTS] {
  return sessions;
}

bool anyAuthed() {
  for (const Session& session : sessions) {
    if (session.used && session.authed) return true;
  }
  return false;
}

}
}
