cmake_minimum_required(VERSION 3.22)

project(telemetry_report VERSION 1.0.0 LANGUAGES CXX)

# Library target
add_library(sensor_statistics STATIC src/sensor_statistics.cpp)
target_include_directories(sensor_statistics
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(sensor_statistics PUBLIC cxx_std_17)
set_target_properties(sensor_statistics PROPERTIES CXX_EXTENSIONS OFF)

# Executable target
add_executable(telemetry_report app/telemetry_report_main.cpp)
target_link_libraries(telemetry_report PRIVATE sensor_statistics)
set_target_properties(telemetry_report PROPERTIES CXX_EXTENSIONS OFF)
