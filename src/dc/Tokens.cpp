#include "Tokens.h"

#include <OtaUpdater.h>

#include "Platform.h"

namespace DeviceControl {
namespace Tokens {

namespace {

constexpr size_t LENGTH = Config::TOKEN_LENGTH;

char tokens[Config::MAX_TOKENS][LENGTH + 1] = {};

bool same(const char* a, const char* b) {
  uint8_t diff = 0;
  for (size_t i = 0; i < LENGTH; i++) diff |= a[i] ^ b[i];
  return diff == 0;
}

void removeAt(size_t index) {
  for (size_t i = index; i + 1 < Config::MAX_TOKENS; i++) memcpy(tokens[i], tokens[i + 1], LENGTH + 1);
  tokens[Config::MAX_TOKENS - 1][0] = '\0';
}

void save() {
  char joined[Config::MAX_TOKENS * LENGTH + 1] = {};
  {
    Lock lock;
    size_t length = 0;
    for (const auto& token : tokens) {
      if (!token[0]) continue;
      memcpy(joined + length, token, LENGTH);
      length += LENGTH;
    }
  }
  OtaUpdater::saveSecret(Config::TOKENS_SECRET, joined);
}

void generate(char (&token)[LENGTH + 1]) {
  for (size_t i = 0; i < LENGTH; i += 8) {
    snprintf(token + i, 9, "%08x", static_cast<unsigned>(randomWord()));
  }
}

}

void load() {
  String stored = OtaUpdater::loadSecret(Config::TOKENS_SECRET);
  Lock lock;
  for (size_t i = 0; i < Config::MAX_TOKENS; i++) {
    size_t offset = i * LENGTH;
    if (stored.length() < offset + LENGTH) break;
    memcpy(tokens[i], stored.c_str() + offset, LENGTH);
    tokens[i][LENGTH] = '\0';
  }
}

bool isValid(const char* token) {
  if (!token || strnlen(token, LENGTH + 1) != LENGTH) return false;
  Lock lock;
  bool valid = false;
  for (const auto& stored : tokens) {
    if (stored[0] && same(stored, token)) valid = true;
  }
  return valid;
}

void issue(char (&token)[LENGTH + 1], char (&dropped)[LENGTH + 1]) {
  generate(token);
  dropped[0] = '\0';
  {
    Lock lock;
    size_t slot = 0;
    while (slot < Config::MAX_TOKENS && tokens[slot][0]) slot++;
    if (slot == Config::MAX_TOKENS) {
      memcpy(dropped, tokens[0], LENGTH + 1);
      removeAt(0);
      slot = Config::MAX_TOKENS - 1;
    }
    memcpy(tokens[slot], token, LENGTH + 1);
  }
  save();
}

bool remove(const char* token) {
  bool removed = false;
  {
    Lock lock;
    for (size_t i = 0; i < Config::MAX_TOKENS; i++) {
      if (tokens[i][0] && same(tokens[i], token)) {
        removeAt(i);
        removed = true;
        break;
      }
    }
  }
  if (removed) save();
  return removed;
}

}
}
