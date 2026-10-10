#pragma once

#include <Arduino.h>

namespace DeviceControl {
namespace Config {

constexpr int PROTOCOL = 1;
constexpr uint16_t PORT = 80;
constexpr const char* SERVICE = "espctl";
constexpr const char* TOKENS_SECRET = "dc_tokens";
constexpr size_t MAX_CLIENTS = 3;
constexpr size_t MAX_TOKENS = 4;
constexpr size_t TOKEN_LENGTH = 32;
constexpr size_t PIN_LENGTH = 4;
constexpr size_t MAX_PIN_SYMBOLS = 10;
constexpr size_t MAX_MESSAGE = 512;
constexpr size_t QUEUE_SIZE = 12;
constexpr uint16_t KEEPALIVE_S = 15;
constexpr uint32_t AUTH_TIMEOUT_MS = 10000;
constexpr uint32_t PAIRING_TIMEOUT_MS = 60000;
constexpr uint8_t PIN_ATTEMPTS = 3;
constexpr uint32_t WIFI_RETRY_MS = 60000;
constexpr uint32_t CLEANUP_MS = 1000;
constexpr uint32_t RESTART_DELAY_MS = 500;

}
}
