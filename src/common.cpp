#include "common.hpp"

std::unordered_map<uintptr_t, int> ContentInjector::enum_value_sizes;
std::unordered_map<uintptr_t, std::unordered_map<size_t, uint64_t>> ContentInjector::enum_labels_to_value;
std::unordered_map<uintptr_t, std::unordered_map<uint64_t, reframework::API::ManagedObject*>> ContentInjector::enum_values_to_label;
std::shared_mutex ContentInjector::g_enum_edit_mutex{};
