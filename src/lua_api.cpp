#include "lua_api.hpp"
#include "common.hpp"
#include "utils.hpp"

#include <codecvt>
#include <inttypes.h>
#include <locale>
#include <string>
#include <string_view>

using API = reframework::API;
using namespace ContentInjector;

lua_State* g_lua{nullptr};

void ensure_enum_type_cache_exists(reframework::API::TypeDefinition* enumType) {
    auto typeId = (uintptr_t)enumType->get_runtime_type();
    if (enum_labels_to_value.find(typeId) == enum_labels_to_value.end()) {
        enum_labels_to_value[typeId] = {};
        enum_values_to_label[typeId] = {};
        auto underlyingType = enumType->get_underlying_type()->get_full_name();
        if (underlyingType == "System.UInt64" || underlyingType == "System.Int64") {
            enum_value_sizes[typeId] = 8;
        } else if (underlyingType == "System.UInt16" || underlyingType == "System.Int16") {
            enum_value_sizes[typeId] = 2;
        } else {
            enum_value_sizes[typeId] = 4;
        }
        // API::get()->log_info("Added cache structs for new enum with size %d", enum_value_sizes[typeId]);
    }
}

void ContentInjector::add_enum_entries(sol::object type, sol::table table) {
    API::TypeDefinition* typeObj;
    if (type.is<std::string>()) {
        typeObj = API::get()->tdb()->find_type(type.as<std::string>());
    } else if (type.is<API::TypeDefinition*>()) {
        typeObj = type.as<API::TypeDefinition*>();
    } else {
        throw sol::error("content_injector.add_enum_entry: expected params = string|TypeDefinition type, string label, number value");
    }

    if (typeObj == nullptr || !typeObj->is_enum()) {
        throw sol::error("content_injector.add_enum_entry: parameter 1 must be enum type");
    }

    {
        std::unique_lock _{g_enum_edit_mutex};
        ensure_enum_type_cache_exists(typeObj);

        auto typeId = (uintptr_t)typeObj->get_runtime_type();
        auto l_to_v = enum_labels_to_value[typeId];
        auto v_to_l = enum_values_to_label[typeId];

        for (auto entry : table.pairs()) {
            auto label = entry.first.as<std::string>();
            auto value = entry.second.as<int64_t>();

            // TODO check if we already have the entry

            auto wide = lua_str_to_wide(label);
            auto str = API::get()->create_managed_string(wide.data());
            str->add_ref();

            l_to_v[std::hash<std::wstring_view>{}(wide)] = value;
            v_to_l[value] = str;
            if (DEBUG) {
                API::get()->log_info("Added enum override %s %d: %s (w: %ls) -> %d", typeObj->get_full_name().c_str(), typeId, label.data(),
                    wide.data(), value);
            }
        }
    }
}

void ContentInjector::add_enum_entry(sol::object type, std::string label, int64_t value) {
    API::TypeDefinition* typeObj;
    if (type.is<std::string>()) {
        typeObj = API::get()->tdb()->find_type(type.as<std::string>());
    } else if (type.is<API::TypeDefinition*>()) {
        typeObj = type.as<API::TypeDefinition*>();
    } else {
        throw sol::error("content_injector.add_enum_entry: expected params = string|TypeDefinition type, string label, number value");
    }

    if (typeObj == nullptr || !typeObj->is_enum()) {
        throw sol::error("content_injector.add_enum_entry: parameter 1 must be enum type");
    }

    {
        std::unique_lock _{g_enum_edit_mutex};
        ensure_enum_type_cache_exists(typeObj);
        auto typeId = (uintptr_t)typeObj->get_runtime_type();
        auto l_to_v = enum_labels_to_value[typeId];
        auto v_to_l = enum_values_to_label[typeId];

        auto wide = lua_str_to_wide(label);
        auto whash = std::hash<std::wstring_view>{}(wide);
        enum_labels_to_value[typeId][whash] = value;
        auto str = API::get()->create_managed_string(wide.data());
        str->add_ref();
        enum_values_to_label[typeId][value] = str;
        if (DEBUG) {
            API::get()->log_info("Added enum override %s %d: %s (%ls %lld => %lld)", typeObj->get_full_name().c_str(), typeId, label.data(),
                wide.data(), whash, value);
        }
    }
}

void on_lua_state_created(lua_State* l) {
    API::LuaLock _{};

    API::get()->log_info("Content injector: lua state created");

    g_lua = l;

    sol::state_view lua{g_lua};

    auto injector = lua.create_table();

    injector["add_enum_entry"] = &add_enum_entry;
    injector["add_enum_entries"] = &add_enum_entries;

    lua["content_injector"] = injector;
    lua["content_injector_test"] = sol::new_table{};
}

void on_ref_lua_state_destroyed(lua_State* l) try { g_lua = nullptr; } catch (const std::exception& e) {
    API::get()->log_error("[reframework-d2d] [on_ref_lua_state_destroyed] %s", e.what());
}

void ContentInjector::lua_setup(const REFrameworkPluginFunctions* const functions) {
    functions->on_lua_state_created(on_lua_state_created);
    functions->on_lua_state_destroyed(on_ref_lua_state_destroyed);
}