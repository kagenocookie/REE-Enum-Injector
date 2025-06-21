#include "hooks.hpp"

#include <string_view>

#include "common.hpp"
#include "utils.hpp"

using API = reframework::API;

using namespace ContentInjector;

API::Method* m_runtimeType_get_FullName;

thread_local bool parse_return_override;
thread_local API::ManagedObject* tostring_return_override = nullptr;

const int enum_value_offset = 0x10;

int pre_TryParseInternal(int argc, void** argv, REFrameworkTypeDefinitionHandle* arg_tys, unsigned long long ret_addr) {
    static auto system_activator_type = API::get()->tdb()->find_type("System.Activator");
    static auto create_instance_func = system_activator_type->find_method("CreateInstance(System.Type)");

    // argv[1] = enumType System.RuntimeType
    // argv[2] = value System.String
    // argv[3] = ignoreCase bool
    // argv[4] = byref__result object
    auto enumType = (API::ManagedObject*)argv[1]; // System.RuntimeType
    auto typeId = (uintptr_t)enumType;

    std::shared_lock _{g_enum_edit_mutex};
    if (auto it1 = enum_labels_to_value.find(typeId); it1 != enum_labels_to_value.end()) {
        auto l_to_v = it1->second;
        auto str = get_string_view((API::ManagedObject*)argv[2]);
        auto hash = std::hash<std::wstring_view>{}(str);

        if (DEBUG) {
            auto typeName = get_string_view((API::ManagedObject*)m_runtimeType_get_FullName->call(argv[0], enumType));
            API::get()->log_info("Requested overridden enum %ls %d: %ls; whash: %lld", typeName.data(), typeId, str.data(), hash);
        }

        if (auto it = l_to_v.find(hash); it != l_to_v.end()) {
            auto value = it->second;

            auto boxedValue = create_instance_func->call<API::ManagedObject*>(argv[0], enumType);
            // boxedValue->add_ref();
            auto enumSize = enum_value_sizes[typeId];
            switch (enumSize) {
                case 2: *(uint16_t*)(boxedValue + enum_value_offset) = (uint16_t)value; break;
                default:
                case 4: *(uint32_t*)(boxedValue + enum_value_offset) = (uint32_t)value; break;
                case 8: *(uint64_t*)(boxedValue + enum_value_offset) = (uint64_t)value; break;
            }

            auto valuePtr = (void**)argv[4];
            (*valuePtr) = boxedValue;

            if (DEBUG) {
                auto typeName = get_string_view((API::ManagedObject*)m_runtimeType_get_FullName->call(argv[0], enumType));
                API::get()->log_info("Returning enum override %ls: %ls -> %d", typeName.data(), str.data(), value);
            }
            parse_return_override = true;
            return REFRAMEWORK_HOOK_SKIP_ORIGINAL;
        }
    }

    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

void post_TryParseInternal(void** ret_val, REFrameworkTypeDefinitionHandle ret_ty, unsigned long long ret_addr) {
    if (parse_return_override) {
        API::get()->log_info("enum post hook triggered");
        (*ret_val) = (void*)true;
        parse_return_override = false;
    }
}

int pre_ToString(int argc, void** argv, REFrameworkTypeDefinitionHandle* arg_tys, unsigned long long ret_addr) {
    auto enumType = (API::ManagedObject*)argv[1]; // boxed enum
    auto typeId = (uintptr_t)enumType->get_type_definition()->get_runtime_type();

    {
        std::shared_lock _{g_enum_edit_mutex};
        if (auto it1 = enum_values_to_label.find(typeId); it1 != enum_values_to_label.end()) {
            auto v_to_l = it1->second;

            uint64_t value;
            auto enumSize = enum_value_sizes[typeId];
            switch (enumSize) {
                case 2: value = (uint64_t)*(uint16_t*)(enumType + enum_value_offset); break;
                default:
                case 4: value = (uint64_t)*(uint32_t*)(enumType + enum_value_offset); break;
                case 8: value = *(uint64_t*)(enumType + enum_value_offset); break;
            }

            if (DEBUG) {
                auto typeName = get_string_view((API::ManagedObject*)m_runtimeType_get_FullName->call(argv[0], typeId));
                API::get()->log_info("Enum override ToString requested %ls: %lld", typeName.data(), value);
            }

            if (auto it2 = v_to_l.find(value); it2 != v_to_l.end()) {
                tostring_return_override = it2->second;
                return REFRAMEWORK_HOOK_SKIP_ORIGINAL;
            }
        }
    }

    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}
void post_ToString(void** ret_val, REFrameworkTypeDefinitionHandle ret_ty, unsigned long long ret_addr) {
    if (tostring_return_override) {
        (*ret_val) = tostring_return_override;
        tostring_return_override = nullptr;
    }
}

void ContentInjector::hooks_init(const reframework::API::TDB* const tdb) {
    tdb->find_method("System.Enum", "TryParseInternal")->add_hook(pre_TryParseInternal, post_TryParseInternal, false);
    tdb->find_method("System.Enum", "ToString")->add_hook(pre_ToString, post_ToString, false);
    m_runtimeType_get_FullName = tdb->find_method("System.RuntimeType", "get_FullName");
}
