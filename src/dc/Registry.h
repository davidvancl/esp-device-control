#pragma once

#include <vector>

#include "../DeviceControl.h"

namespace DeviceControl {
namespace Registry {

struct Entry {
  const char* id = nullptr;
  Getter getter;
  Setter setter;
};

Entry& entry(const char* id);

Entry* find(const char* id);

std::vector<Entry>& all();

}
}
