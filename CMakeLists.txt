cmake_minimum_required(VERSION 3.29)
project(wcc_http C)

set(CMAKE_C_STANDARD 23)
set(C_STANDARD_REQUIRED ON)

#
# Options
#
option(BUILD_SHARED_LIBS "Build shared libraries (.dll/.so) instead of static ones (.lib/.a)" OFF)

#
# Default Values
#
add_library(common INTERFACE)
if (MSVC)
    target_compile_options(common INTERFACE /W3 /experimental:c11atomics)
    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(common INTERFACE /O2 /DNDEBUG)
    endif ()
    if (NOT BUILD_SHARED_LIBS)
        target_compile_options(common INTERFACE /DWCC_HTTP_STATIC_LIB)
    endif ()
else ()
    target_compile_options(common INTERFACE -Wall -Wextra)
    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(common INTERFACE -O3 -DNDEBUG)
    endif ()
    if (NOT BUILD_SHARED_LIBS)
        target_compile_options(common INTERFACE -DWCC_HTTP_STATIC_LIB)
    endif ()
endif ()

add_library(highway
        "core/src/hiw.c"
        "core/src/hiw_std.c"
        "core/src/hiw_thread.c"
        "core/src/hiw_socket.c"
        "core/src/hiw_server.c"
)
target_include_directories(highway PUBLIC core/include)
target_link_libraries(highway PRIVATE common)
if (BUILD_SHARED_LIBS)
    target_link_libraries(highway PRIVATE wsock32 ws2_32)
endif ()

add_library(highway_servlet
        "servlet/src/hiw_servlet.c"
)
target_include_directories(highway_servlet PUBLIC core/include)
target_include_directories(highway_servlet PUBLIC servlet/include)
target_link_libraries(highway_servlet PRIVATE common)
if (BUILD_SHARED_LIBS)
    target_link_libraries(highway_servlet PRIVATE wsock32 ws2_32)
endif ()

#
# Hello World Executable
#
add_executable(hello_world "examples/hello_world/main.c")
target_include_directories(hello_world PUBLIC core/include)
target_link_libraries(hello_world PRIVATE common)
target_link_libraries(hello_world PRIVATE highway highway_servlet wsock32 ws2_32)

