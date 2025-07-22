#include "hooks.hpp"

#include <filesystem>
#include <fstream>
#include <string_view>

#include "common.hpp"
#include "lua_api.hpp"
#include "utils.hpp"

using API = reframework::API;

using namespace ContentInjector;

API::Method* m_runtimeType_get_FullName;

thread_local bool parse_return_override;
thread_local API::ManagedObject* tostring_return_override = nullptr;
thread_local API::ManagedObject* get_names_return = nullptr;
thread_local API::ManagedObject* get_values_return = nullptr;

std::unordered_map<uintptr_t, API::ManagedObject*> enum_name_arrays;
std::unordered_map<uintptr_t, API::ManagedObject*> enum_value_arrays;

const int enum_value_offset = 0x10;
const int array_item_ptr_offset = 0x10;

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

void loadConfigData() {
    static auto system_activator_type = API::get()->tdb()->find_type("System.Activator");
    static auto create_instance_func = system_activator_type->find_method("CreateInstance(System.Type)");
    static auto array_get_Length = API::get()->tdb()->find_method("System.Array", "get_Length");
    static auto array_get_item = API::get()->tdb()->find_method("System.Array", "System.Collections.IList.get_Item");
    static auto array_set_item = API::get()->tdb()->find_method("System.Array", "System.Collections.IList.set_Item");
    static auto enum_get_Names = API::get()->tdb()->find_method("System.Enum", "GetNames");
    static auto enum_get_Values = API::get()->tdb()->find_method("System.Enum", "GetValues");
    static auto type_str = API::get()->tdb()->find_type("System.String");

    API::get()->log_info("Loading enum configs...");
    std::filesystem::path path = std::filesystem::path("reframework") / "data" / "usercontent" / "injected_enums";
    std::filesystem::create_directories(path);
    const auto vm = API::get()->get_vm_context();

    uintptr_t curTypeId;
    std::unordered_map<uintptr_t, API::TypeDefinition*> typedefs{};

    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        const auto entry_path = entry.path();
        if (entry_path.extension() != ".txt")
            continue;

        API::get()->log_info("Enum source file: %ls", entry_path.c_str());
        std::ifstream file(entry_path);
        try {
            std::string line;
            while (std::getline(file, line)) {
                if (line.length() == 0 || line[0] == '#')
                    continue;

                // classname declaration
                if (line[0] == '@') {
                    auto tdef = API::get()->tdb()->find_type(line.substr(1));
                    curTypeId = (uintptr_t)tdef->get_runtime_type();
                    if (typedefs.find(curTypeId) == typedefs.end()) {
                        ContentInjector::ensure_enum_type_cache_exists(tdef);
                    }
                    typedefs[curTypeId] = tdef;
                    continue;
                }
                // skip blank lines
                if (line.find_first_not_of(" \t") == std::string::npos)
                    continue;

                // enum entry line
                if (int space = line.find(' '); space != std::string::npos) {
                    auto name = line.substr(0, space);
                    auto value = std::stoll(line.substr(space + 1), NULL, 10);
                    auto wname = lua_str_to_wide(name);
                    auto whash = std::hash<std::wstring_view>{}(wname);
                    auto managed_str = API::get()->create_managed_string(wname.data());
                    managed_str->add_ref();
                    enum_values_to_label[curTypeId][value] = managed_str;
                    enum_labels_to_value[curTypeId][whash] = value;
                }
            }
        } catch (...) {
            API::get()->log_error("Failed to handle file: %s", entry_path.c_str());
        }
        curTypeId = 0;
        file.close();
    }

    API::get()->log_info("Preparing injected arrays...");
    for (const auto& entryType : enum_labels_to_value) {
        auto typeId = entryType.first;
        auto type = typedefs[entryType.first];
        auto orgNames = (API::ManagedObject*)enum_get_Names->call(vm, type->get_runtime_type());
        auto orgValues = (API::ManagedObject*)enum_get_Values->call(vm, type->get_runtime_type());
        int orgCount = (int)array_get_Length->call(vm, orgNames);

        std::vector<API::ManagedObject*> names{};
        std::vector<API::ManagedObject*> values{};

        auto outNameArray = (API::ManagedObject*)API::get()->create_managed_array(type_str, orgCount + entryType.second.size());
        outNameArray->add_ref();
        auto outValueArray = (API::ManagedObject*)API::get()->create_managed_array(type, orgCount + entryType.second.size());
        outValueArray->add_ref();

        int index = 0;
        API::get()->log_info("Original enum item count: %d", orgCount);
        for (int i = 0; i < orgCount; ++i) {
            array_set_item->call(vm, outNameArray, index, (API::ManagedObject*)array_get_item->call(vm, orgNames, i));
            array_set_item->call(vm, outValueArray, index, (API::ManagedObject*)array_get_item->call(vm, orgValues, i));
            index++;
        }

        for (const auto& entry : entryType.second) {
            auto nameHash = entry.first;
            auto value = entry.second;
            auto labelStringPtr = enum_values_to_label[typeId][value];
            auto boxedValue = create_instance_func->call<API::ManagedObject*>(vm, type->get_runtime_type());
            boxedValue->add_ref();
            auto enumSize = enum_value_sizes[typeId];
            switch (enumSize) {
            case 2: *(uint16_t*)(boxedValue + enum_value_offset) = (uint16_t)value; break;
            default:
            case 4: *(uint32_t*)(boxedValue + enum_value_offset) = (uint32_t)value; break;
            case 8: *(uint64_t*)(boxedValue + enum_value_offset) = (uint64_t)value; break;
            }
            API::get()->log_info("Adding custom enum value %d -> %lld", nameHash, value);
            array_set_item->call(vm, outNameArray, index, (API::ManagedObject*)labelStringPtr);
            array_set_item->call(vm, outValueArray, index, (API::ManagedObject*)boxedValue);
            index++;
        }

        API::get()->log_info("Enum ready");
        enum_name_arrays[entryType.first] = outNameArray;
        enum_value_arrays[entryType.first] = outValueArray;
    }
}


int pre_GetNames(int argc, void** argv, REFrameworkTypeDefinitionHandle* arg_tys, unsigned long long ret_addr) {
    auto enumType = (API::ManagedObject*)argv[1]; // System.RuntimeType
    auto typeId = (uintptr_t)enumType;

    if (auto match = enum_name_arrays.find(typeId); match != enum_name_arrays.end()) {
        get_names_return = match->second;
        return REFRAMEWORK_HOOK_SKIP_ORIGINAL;
    }
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}
void post_GetNames(void** ret_val, REFrameworkTypeDefinitionHandle ret_ty, unsigned long long ret_addr) {
    if (get_names_return) {
        *ret_val = get_names_return;
        get_names_return = nullptr;
    }
}
int pre_GetValues(int argc, void** argv, REFrameworkTypeDefinitionHandle* arg_tys, unsigned long long ret_addr) {
    auto enumType = (API::ManagedObject*)argv[1]; // System.RuntimeType
    auto typeId = (uintptr_t)enumType;

    if (auto match = enum_value_arrays.find(typeId); match != enum_value_arrays.end()) {
        get_values_return = match->second;
        return REFRAMEWORK_HOOK_SKIP_ORIGINAL;
    }
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}
void post_GetValues(void** ret_val, REFrameworkTypeDefinitionHandle ret_ty, unsigned long long ret_addr) {
    if (get_values_return) {
        *ret_val = get_values_return;
        get_values_return = nullptr;
    }
}

void ContentInjector::hooks_init(const reframework::API::TDB* const tdb) {
    auto sys_enum = tdb->find_type("System.Enum");
    loadConfigData();

    m_runtimeType_get_FullName = tdb->find_method("System.RuntimeType", "get_FullName");
    sys_enum->find_method("TryParseInternal")->add_hook(pre_TryParseInternal, post_TryParseInternal, false);
    sys_enum->find_method("ToString")->add_hook(pre_ToString, post_ToString, false);
    sys_enum->find_method("GetNames")->add_hook(pre_GetNames, post_GetNames, false);
    sys_enum->find_method("GetValues")->add_hook(pre_GetValues, post_GetValues, false);
}
