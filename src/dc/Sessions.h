#pragma once

#include <Arduino.h>

#include "Config.h"

namespace DeviceControl {

struct Session {
  bool used = false;
  bool authed = false;
  uint32_t client = 0;
  uint32_t since = 0;
  char token[Config::TOKEN_LENGTH + 1] = {};

  void authorize(const char* value);
  void revoke();
};

namespace Sessions {

Session* open(uint32_t client);

Session* find(uint32_t client);

void release(uint32_t client);

Session (&all())[Config::MAX_CLIENTS];

bool anyAuthed();

}
}
