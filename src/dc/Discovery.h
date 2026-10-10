#pragma once

namespace DeviceControl {
namespace Discovery {

void begin(const char* name, const char* type, const char* id, const char* firmware);

void loop();

}
}
