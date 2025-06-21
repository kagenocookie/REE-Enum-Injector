cmake_minimum_required(VERSION 3.10.0)
project(content_injector VERSION 0.1.0 LANGUAGES C CXX)
include (cmake/CPM.cmake)

CPMAddPackage("gh:nemtrif/utfcpp@4.0.5")
CPMAddPackage("gh:ThePhD/sol2@3.3.0")
CPMAddPackage(
    NAME lua
    GITHUB_REPOSITORY lua/lua
    VERSION 5.4.3
    DOWNLOAD_ONLY YES
)

if (lua_ADDED)
    FILE(GLOB lua_sources ${lua_SOURCE_DIR}/*.c)
    list(REMOVE_ITEM lua_sources "${lua_SOURCE_DIR}/lua.c" "${lua_SOURCE_DIR}/luac.c" "${lua_SOURCE_DIR}/onelua.c")
    add_library(lua STATIC ${lua_sources})

    target_include_directories(lua
        PUBLIC
        $<BUILD_INTERFACE:${lua_SOURCE_DIR}>
    )
endif()

add_library(content_injector SHARED
    src/main.cpp
    src/common.hpp
    src/common.cpp
    src/utils.hpp
    src/utils.cpp
    src/hooks.hpp
    src/hooks.cpp
    src/lua_api.hpp
    src/lua_api.cpp
)

target_include_directories(content_injector PRIVATE
    src
    include
)

target_compile_features(content_injector PRIVATE cxx_std_20)
target_link_libraries(content_injector PRIVATE utf8cpp sol2::sol2 lua)

install(
    TARGETS content_injector
    DESTINATION bin
    COMPONENT content_injector
)