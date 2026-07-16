cmake_minimum_required(VERSION 3.16)
project(wanderer_seed LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
find_package(Threads REQUIRED)
target_link_libraries(wanderer_seed PRIVATE nlohmann_json::nlohmann_json Threads::Threads)
