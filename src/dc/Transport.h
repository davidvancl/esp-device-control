#pragma once

#include <Arduino.h>

namespace DeviceControl {
namespace Transport {

enum class Kind : uint8_t { Connect, Disconnect, Message };

struct Incoming {
  Kind kind = Kind::Message;
  uint32_t client = 0;
  String data;
};

void begin();

bool poll(Incoming& out);

void send(uint32_t client, const String& text);

void close(uint32_t client);

bool isConnected(uint32_t client);

void cleanup();

}
}
