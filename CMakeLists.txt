cmake_minimum_required(VERSION 3.29)
project(highway C)

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

#
# Highway Core
#
add_library(highway
        "core/src/highway.c"
        "core/src/hiw_std.c"
        "core/src/hiw_thread.c"
        "core/src/hiw_socket.c"
        "core/src/hiw_server.c"
        "core/src/hiw_mimetypes.c"
)
target_include_directories(highway PUBLIC core/include)
target_link_libraries(highway PRIVATE common)
if (BUILD_SHARED_LIBS)
    target_link_libraries(highway PRIVATE wsock32 ws2_32)
endif ()

#
# Highway Servlet
#
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
# Highway Boot
#
add_library(highway_boot
        "boot/src/hiw_boot.c"
)
target_include_directories(highway_boot PUBLIC core/include)
target_include_directories(highway_boot PUBLIC servlet/include)
target_include_directories(highway_boot PUBLIC boot/include)
target_link_libraries(highway_boot PRIVATE common)
if (BUILD_SHARED_LIBS)
    target_link_libraries(highway_boot PRIVATE wsock32 ws2_32)
endif ()

#
# Hello World Executable
#
add_executable(examples_hello_world "examples/hello_world/main.c")
target_include_directories(examples_hello_world PUBLIC core/include)
target_include_directories(examples_hello_world PUBLIC servlet/include)
target_link_libraries(examples_hello_world PRIVATE common)
target_link_libraries(examples_hello_world PRIVATE highway highway_servlet wsock32 ws2_32)

#
# Static Content Executable
#
add_executable(examples_static
        "examples/static/main.c"
        "examples/static/static_cache.c"
)
target_include_directories(examples_static PUBLIC core/include)
target_include_directories(examples_static PUBLIC servlet/include)
target_link_libraries(examples_static PRIVATE common)
target_link_libraries(examples_static PRIVATE highway highway_servlet wsock32 ws2_32)

#
# Simple Json REST service using Highway Boot
#
add_executable(examples_boot
        "examples/boot/main.c"
)
target_include_directories(examples_boot PUBLIC core/include)
target_include_directories(examples_boot PUBLIC servlet/include)
target_include_directories(examples_boot PUBLIC boot/include)
target_link_libraries(examples_boot PRIVATE common)
target_link_libraries(examples_boot PRIVATE highway highway_servlet highway_boot wsock32 ws2_32)

#
# A Json Cache using Highway Boot
#
add_executable(examples_jc
        "examples/jc/main.c"
)
target_include_directories(examples_jc PUBLIC core/include)
target_include_directories(examples_jc PUBLIC servlet/include)
target_include_directories(examples_jc PUBLIC boot/include)
target_link_libraries(examples_jc PRIVATE common)
target_link_libraries(examples_jc PRIVATE highway highway_servlet highway_boot wsock32 ws2_32)
