#pragma once

#include <Arduino.h>
#include <functional>

namespace DeviceControl {
namespace Pairing {

enum class Method : uint8_t { None, Pin, Confirm };

enum class PinResult : uint8_t { Accepted, Wrong, Rejected, Invalid };

void usePin(std::function<void(const char*)> show, std::function<void()> hide);

void useConfirm(std::function<void()> start, std::function<void()> stop);

Method method();

const char* methodName();

bool active();

bool isOwner(uint32_t client);

uint32_t owner();

void start(uint32_t client);

PinResult checkPin(uint32_t client, const char* pin);

bool canConfirm();

bool expired();

void end();

}
}
