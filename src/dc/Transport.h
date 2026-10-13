#pragma once

#include <Arduino.h>
#include <functional>

namespace DeviceControl {
namespace Transport {

using DataHandler = std::function<void(const uint8_t*, size_t)>;

enum class Kind : uint8_t { Connect, Disconnect, Message };

struct Incoming {
  Kind kind = Kind::Message;
  uint32_t client = 0;
  String data;
};

void begin();

void onData(DataHandler handler);

void setAuthorized(const uint32_t* clients, size_t length);

bool poll(Incoming& out);

void send(uint32_t client, const String& text);

void close(uint32_t client);

bool isConnected(uint32_t client);

void cleanup();

}
}
