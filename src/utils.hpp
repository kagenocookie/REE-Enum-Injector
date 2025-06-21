#pragma once

#include <reframework/API.hpp>
#include <string_view>

namespace ContentInjector {
std::wstring_view get_string_view(const reframework::API::ManagedObject* systemString);
void string_cache_init(const reframework::API::TDB* const tdb);
std::wstring lua_str_to_wide(const std::string_view& str);
} // namespace ContentInjector
