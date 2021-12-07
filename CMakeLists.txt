cmake_minimum_required(VERSION 3.20)
project(mosquitto_cpp)

set(CMAKE_CXX_STANDARD 11)

option(WITH_STATIC_LIBRARIES "Build static versions of the libmosquitto/pp libraries?" ON)
option(WITH_CJSON "Build with cJSON support (required for dynamic security plugin and useful for mosquitto_sub)?" OFF)
option(WITH_PIC "Build the static library with PIC (Position Independent Code) enabled archives?" ON)

option(WITH_CLIENTS "Build clients?" OFF)
option(WITH_BROKER "Build broker?" OFF)
option(WITH_APPS "Build apps?" OFF)
option(DOCUMENTATION "Build documentation?" OFF)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/mosquitto build/mosquitto)

add_library(mosquitto_cpp src/Client.cpp)
target_link_libraries(mosquitto_cpp libmosquitto_static)
target_include_directories(mosquitto_cpp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../mosquitto/include)

add_executable(client example/client_example.cpp)
include_directories(client include)
target_link_libraries(client mosquitto_cpp)

