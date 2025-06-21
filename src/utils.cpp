#include "utils.hpp"

#include <Windows.h>

using API = reframework::API;

std::wstring_view ContentInjector::get_string_view(const API::ManagedObject* systemString) {
    static API::TypeDefinition *stringType = API::get()->tdb()->find_type("System.String");
    static int stringLenOffset = stringType->get_fieldptr_offset();
    static int stringDataOffset = stringType->get_fieldptr_offset() + 4;

    int length = *(int*)(systemString + stringLenOffset);
    if (length <= 0)
        return L"";

    auto dataptr = (uintptr_t)(systemString + stringDataOffset);
    std::wstring_view str;
    str = std::wstring_view((wchar_t*)dataptr, (size_t)length);
    return str;
}

std::wstring ContentInjector::lua_str_to_wide(const std::string_view& str) {
    int newLength = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.length(), nullptr, 0);
    std::wstring wideStr(newLength, 0);

    MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.length(), (LPWSTR)wideStr.c_str(), newLength);
    return wideStr;
}
