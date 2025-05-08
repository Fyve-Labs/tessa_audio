cmake_minimum_required(VERSION 3.10)
project(tessa_audio VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set CMake policies
cmake_policy(SET CMP0135 NEW) # Use timestamps from extraction time for downloads

# Add custom CMake module path
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

# Find required packages using our custom Find modules
find_package(PortAudio REQUIRED)
find_package(ZeroMQ REQUIRED)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${ZeroMQ_INCLUDE_DIRS}
    ${PORTAUDIO_INCLUDE_DIRS}
)

# Handle nlohmann/json directly instead of using FetchContent
set(JSON_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/json_include")
file(MAKE_DIRECTORY "${JSON_INCLUDE_DIR}/nlohmann")

if(NOT EXISTS "${JSON_INCLUDE_DIR}/nlohmann/json.hpp")
    message(STATUS "Downloading nlohmann/json...")
    file(DOWNLOAD
        https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp
        "${JSON_INCLUDE_DIR}/nlohmann/json.hpp"
        SHOW_PROGRESS
    )
endif()

include_directories(${JSON_INCLUDE_DIR})

# Set source files
set(LIB_SOURCES
    src/audio_capture.cpp
    src/zmq_publisher.cpp
    src/zmq_handler.cpp
    src/audio_buffer.cpp
    src/device_manager.cpp
)

# Create a shared library
add_library(tessa_audio_lib SHARED ${LIB_SOURCES})
target_link_libraries(tessa_audio_lib
    ${ZeroMQ_LIBRARIES}
    ${PORTAUDIO_LIBRARIES}
    pthread
)

# Add executable target
add_executable(tessa_audio src/main.cpp)
target_link_libraries(tessa_audio tessa_audio_lib)

# Enable testing if we're the main project
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()

# Installation
install(TARGETS tessa_audio tessa_audio_lib
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
install(DIRECTORY include/ DESTINATION include) 