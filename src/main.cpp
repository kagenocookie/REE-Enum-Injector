#include <reframework/API.hpp>

#include "hooks.hpp"
#include "lua_api.hpp"
#include "utils.hpp"

using API = reframework::API;
using namespace ContentInjector;

extern "C" __declspec(dllexport) bool reframework_plugin_initialize(const REFrameworkPluginInitializeParam* param) {
    API::initialize(param);

    const auto functions = param->functions;
    const auto tdb = API::get()->tdb();

    functions->log_info("Content injector: initing");

    hooks_init(tdb);
    lua_setup(functions);

    return true;
}

extern "C" __declspec(dllexport) void reframework_plugin_required_version(REFrameworkPluginVersion* version) {
    version->major = REFRAMEWORK_PLUGIN_VERSION_MAJOR;
    version->minor = 14;
    version->patch = REFRAMEWORK_PLUGIN_VERSION_PATCH;
}
