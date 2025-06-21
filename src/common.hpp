#pragma once

#include <reframework/API.hpp>
#include <shared_mutex>
#include <unordered_map>

namespace ContentInjector {
extern std::unordered_map<uintptr_t, int> enum_value_sizes;
extern std::unordered_map<uintptr_t, std::unordered_map<size_t, uint64_t>> enum_labels_to_value;
extern std::unordered_map<uintptr_t, std::unordered_map<uint64_t, reframework::API::ManagedObject*>> enum_values_to_label;
extern std::shared_mutex g_enum_edit_mutex;
const bool DEBUG = false;
} // namespace ContentInjector
