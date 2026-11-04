cmake_minimum_required(VERSION 3.16)
project(florr_server C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O2")
# float maths must round the same way in every build
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffp-contract=off")

# everything but the entry point
file(GLOB_RECURSE CORE_SRC ${CMAKE_SOURCE_DIR}/src/world/*.cpp ${CMAKE_SOURCE_DIR}/src/game/*.cpp)
add_library(gamecore STATIC ${CORE_SRC})
target_include_directories(gamecore PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/../shared)

# one process serving the page and the WebSocket on one port
set(UWS ${CMAKE_SOURCE_DIR}/third_party/uWebSockets)
set(USOCKETS ${UWS}/uSockets)
file(GLOB USOCKETS_C ${USOCKETS}/src/*.c ${USOCKETS}/src/eventing/*.c)
add_library(usockets STATIC ${USOCKETS_C} ${USOCKETS}/src/crypto/sni_tree.cpp)
target_include_directories(usockets PUBLIC ${USOCKETS}/src)
target_compile_definitions(usockets PUBLIC LIBUS_NO_SSL=1)
add_executable(server ${CMAKE_SOURCE_DIR}/src/platform/native.cpp)
target_include_directories(server PRIVATE ${UWS}/src)
target_link_libraries(server PRIVATE gamecore usockets z pthread)
