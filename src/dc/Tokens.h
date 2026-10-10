#pragma once

#include "Config.h"

namespace DeviceControl {
namespace Tokens {

void load();

bool isValid(const char* token);

void issue(char (&token)[Config::TOKEN_LENGTH + 1], char (&dropped)[Config::TOKEN_LENGTH + 1]);

bool remove(const char* token);

}
}
