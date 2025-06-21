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
