cmake_minimum_required(VERSION 3.16)
project(gbx_http_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(gbx_http_app
    src/main.cpp
    src/httpserver.cpp
    src/scheduler.cpp
    src/soundmanager.cpp
    src/mcp23017driver.cpp
    src/display/LCD2004.cpp
)

target_include_directories(gbx_http_app
    PRIVATE
        src
)

target_include_directories(gbx_http_app
    SYSTEM PRIVATE
        src/libs
)

target_link_libraries(gbx_http_app
    pthread
)

if(MSVC)
    target_compile_options(gbx_http_app PRIVATE /W4)
else()
    target_compile_options(gbx_http_app PRIVATE -Wall -Wextra -Wpedantic -Wno-psabi)
endif()