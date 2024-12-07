cmake_minimum_required(VERSION 3.25.1)
project(highway C CXX)

set(CMAKE_C_STANDARD 23)
set(C_STANDARD_REQUIRED ON)

#
# Options
#
option(BUILD_SHARED_LIBS "Build shared libraries (.dll/.so) instead of static ones (.lib/.a)" OFF)

#
# Library defaults
#
set(HIW_MAX_HEADER_SIZE 8192 CACHE STRING "The maximum header size allowed")
set(HIW_MAX_HEADERS_COUNT 32 CACHE STRING "The maximum number of headers parsed on incoming request")
set(HIW_WRITE_SERVER_HEADER 1 CACHE STRING "Should the library automatically add the server header in the  response header")
set(HIW_WRITE_SERVER_VERSION 1 CACHE STRING "Should the library automatically add the server version in the response header")
set(HIW_THREAD_WAIT_DEFAULT_TIMEOUT 30000 CACHE STRING "How long the servlet is waiting for threads on shutdown by default")

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
        target_compile_options(common INTERFACE /DHIW_HTTP_STATIC_LIB)
    endif ()
    target_compile_options(common INTERFACE /DHIW_MAX_HEADER_SIZE=${HIW_MAX_HEADER_SIZE})
    target_compile_options(common INTERFACE /DHIW_MAX_HEADERS_COUNT=${HIW_MAX_HEADERS_COUNT})
    target_compile_options(common INTERFACE /DHIW_WRITE_SERVER_HEADER=${HIW_WRITE_SERVER_HEADER})
    target_compile_options(common INTERFACE /DHIW_WRITE_SERVER_VERSION=${HIW_WRITE_SERVER_VERSION})
    target_compile_options(common INTERFACE /DHIW_THREAD_WAIT_DEFAULT_TIMEOUT=${HIW_THREAD_WAIT_DEFAULT_TIMEOUT})

    set(SOCKET_LIBRARIES wsock32 ws2_32)
else ()
    target_compile_options(common INTERFACE -Wall -Wextra)
    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(common INTERFACE -O3 -DNDEBUG)
    endif ()
    if (NOT BUILD_SHARED_LIBS)
        target_compile_options(common INTERFACE -DHIW_HTTP_STATIC_LIB)
    endif ()
    target_compile_options(common INTERFACE -DHIW_MAX_HEADER_SIZE=${HIW_MAX_HEADER_SIZE})
    target_compile_options(common INTERFACE -DHIW_MAX_HEADERS_COUNT=${HIW_MAX_HEADERS_COUNT})
    target_compile_options(common INTERFACE -DHIW_WRITE_SERVER_HEADER=${HIW_WRITE_SERVER_HEADER})
    target_compile_options(common INTERFACE -DHIW_WRITE_SERVER_VERSION=${HIW_WRITE_SERVER_VERSION})
    target_compile_options(common INTERFACE -DHIW_THREAD_WAIT_DEFAULT_TIMEOUT=${HIW_THREAD_WAIT_DEFAULT_TIMEOUT})
    set(SOCKET_LIBRARIES)
endif ()

add_library(common_library INTERFACE)
if (MSVC)
    target_compile_options(common_library INTERFACE /DHIW_HTTP_STATIC_LIB_COMPILING)
else ()
    target_compile_options(common_library INTERFACE -DHIW_HTTP_STATIC_LIB_COMPILING)
endif ()

add_library(statically_link INTERFACE)
if (MSVC)
else ()
    target_link_options(statically_link INTERFACE "-static")
endif ()

add_library(common_cpp INTERFACE)
if (MSVC)
else ()
    # Add exception and compile as static
    # TODO: Should this be configurable?
    target_link_options(common_cpp INTERFACE "-fexceptions")
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
target_link_libraries(highway PRIVATE common common_library ${SOCKET_LIBRARIES})

#
# Highway Servlet
#
add_library(highway_servlet
        "servlet/src/hiw_servlet.c"
        "servlet/src/hiw_file_content.c"
)
target_include_directories(highway_servlet PUBLIC core/include)
target_include_directories(highway_servlet PUBLIC servlet/include)
target_link_libraries(highway_servlet PRIVATE common common_library highway ${SOCKET_LIBRARIES})

#
# Highway Boot
#
add_library(highway_boot
        "boot/src/hiw_boot.c"
)
target_include_directories(highway_boot PUBLIC core/include)
target_include_directories(highway_boot PUBLIC servlet/include)
target_include_directories(highway_boot PUBLIC boot/include)
target_link_libraries(highway_boot PRIVATE common common_library highway highway_servlet statically_link ${SOCKET_LIBRARIES})

#
# Hello World Executable
#
add_executable(examples_hello_world "examples/hello_world/main.c")
target_include_directories(examples_hello_world PUBLIC core/include)
target_include_directories(examples_hello_world PUBLIC servlet/include)
target_link_libraries(examples_hello_world PRIVATE common highway highway_servlet ${SOCKET_LIBRARIES})

#
# Static Content Executable
#
add_executable(examples_static
        "examples/static/main.c"
        "examples/static/static_cache.c"
)
target_include_directories(examples_static PUBLIC core/include)
target_include_directories(examples_static PUBLIC servlet/include)
target_link_libraries(examples_static PRIVATE common highway highway_servlet highway_boot statically_link ${SOCKET_LIBRARIES})

#
# Simple Json REST service using Highway Boot
#
add_executable(examples_boot
        "examples/boot/main.c"
)
target_include_directories(examples_boot PUBLIC core/include)
target_include_directories(examples_boot PUBLIC servlet/include)
target_include_directories(examples_boot PUBLIC boot/include)
target_link_libraries(examples_boot PRIVATE common highway highway_servlet highway_boot ${SOCKET_LIBRARIES})

#
# A Json Cache using Highway Boot Written in C++23
#
add_executable(examples_jc
        "examples/jc/main.cpp"
)
set_property(TARGET examples_jc PROPERTY CXX_STANDARD 23)
target_include_directories(examples_jc PUBLIC core/include)
target_include_directories(examples_jc PUBLIC servlet/include)
target_include_directories(examples_jc PUBLIC boot/include)
target_link_libraries(examples_jc PRIVATE common common_cpp highway highway_servlet highway_boot statically_link ${SOCKET_LIBRARIES})
