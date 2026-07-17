cmake_minimum_required(VERSION 3.16)
project(wanderer_seed LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

add_executable(wanderer_seed
    src/main.cpp
    src/md5.c
    src/sfmt/SFMT.c
)

target_include_directories(wanderer_seed PRIVATE src/sfmt)
target_compile_options(wanderer_seed PRIVATE -march=native)
find_package(Threads REQUIRED)
target_link_libraries(wanderer_seed PRIVATE nlohmann_json::nlohmann_json Threads::Threads)

# Optional GPU search path. Off by default so the tool builds without any
# OpenCL SDK present. Enable with: cmake -S . -B build -DWANDERER_ENABLE_OPENCL=ON
option(WANDERER_ENABLE_OPENCL "Build the optional OpenCL GPU search path" OFF)

if(WANDERER_ENABLE_OPENCL)
    find_package(OpenCL REQUIRED)
    target_compile_definitions(wanderer_seed PRIVATE WANDERER_ENABLE_OPENCL)
    target_link_libraries(wanderer_seed PRIVATE OpenCL::OpenCL)
    message(STATUS "OpenCL GPU search path: ENABLED")
else()
    message(STATUS "OpenCL GPU search path: disabled (set -DWANDERER_ENABLE_OPENCL=ON to enable)")
endif()
