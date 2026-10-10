#include "Registry.h"

namespace DeviceControl {
namespace Registry {

namespace {
std::vector<Entry> entries;
}

Entry* find(const char* id) {
  if (!id) return nullptr;
  for (Entry& item : entries) {
    if (!strcmp(item.id, id)) return &item;
  }
  return nullptr;
}

Entry& entry(const char* id) {
  Entry* existing = find(id);
  if (existing) return *existing;
  entries.emplace_back();
  entries.back().id = id;
  return entries.back();
}

std::vector<Entry>& all() {
  return entries;
}

}
}
