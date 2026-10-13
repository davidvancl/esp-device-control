#include "Transport.h"

#include <ESPAsyncWebServer.h>

#include "Config.h"
#include "Platform.h"

namespace DeviceControl {
namespace Transport {

namespace {

AsyncWebServer server(Config::PORT);
AsyncWebSocket socket("/ws");

Incoming queue[Config::QUEUE_SIZE];
size_t head = 0;
size_t count = 0;

DataHandler dataHandler;
uint32_t authorized[Config::MAX_CLIENTS] = {};

bool isAuthorized(uint32_t client) {
  Lock lock;
  for (uint32_t id : authorized) {
    if (id && id == client) return true;
  }
  return false;
}

bool isBinary(void* arg) {
  return static_cast<AwsFrameInfo*>(arg)->message_opcode == WS_BINARY;
}

void push(Kind kind, uint32_t client, String&& data) {
  Lock lock;
  if (count == Config::QUEUE_SIZE) return;
  Incoming& slot = queue[(head + count) % Config::QUEUE_SIZE];
  slot.kind = kind;
  slot.client = client;
  slot.data = std::move(data);
  count++;
}

String frameText(void* arg, uint8_t* data, size_t len) {
  AwsFrameInfo* info = static_cast<AwsFrameInfo*>(arg);
  if (!info->final || info->index != 0 || info->len != len || info->opcode != WS_TEXT) return String();
  if (len > Config::MAX_MESSAGE) return String();
  String text;
  text.reserve(len);
  for (size_t i = 0; i < len; i++) text += static_cast<char>(data[i]);
  return text;
}

void onEvent(AsyncWebSocket*, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      client->keepAlivePeriod(Config::KEEPALIVE_S);
      push(Kind::Connect, client->id(), String());
      break;
    case WS_EVT_DISCONNECT:
      push(Kind::Disconnect, client->id(), String());
      break;
    case WS_EVT_DATA: {
      if (isBinary(arg)) {
        if (dataHandler && isAuthorized(client->id())) dataHandler(data, len);
        break;
      }
      String text = frameText(arg, data, len);
      if (text.length()) push(Kind::Message, client->id(), std::move(text));
      break;
    }
    default:
      break;
  }
}

}

void begin() {
  socket.onEvent(onEvent);
  server.addHandler(&socket);
  server.begin();
}

void onData(DataHandler handler) {
  dataHandler = handler;
}

void setAuthorized(const uint32_t* clients, size_t length) {
  Lock lock;
  for (size_t i = 0; i < Config::MAX_CLIENTS; i++) authorized[i] = i < length ? clients[i] : 0;
}

bool poll(Incoming& out) {
  Lock lock;
  if (count == 0) return false;
  Incoming& slot = queue[head];
  out.kind = slot.kind;
  out.client = slot.client;
  out.data = std::move(slot.data);
  head = (head + 1) % Config::QUEUE_SIZE;
  count--;
  return true;
}

void send(uint32_t client, const String& text) {
  socket.text(client, text);
}

void close(uint32_t client) {
  socket.close(client);
}

bool isConnected(uint32_t client) {
  return socket.client(client) != nullptr;
}

void cleanup() {
  socket.cleanupClients();
}

}
}
