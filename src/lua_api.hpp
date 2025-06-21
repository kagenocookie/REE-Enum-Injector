#pragma once

#include <reframework/API.hpp>
#include <sol/sol.hpp>
#include <string>

namespace ContentInjector {
void add_enum_entries(sol::object type, sol::table table);
void add_enum_entry(sol::object type, std::string label, int64_t value);
void lua_setup(const REFrameworkPluginFunctions* const functions);
} // namespace ContentInjector
