cmake_minimum_required(VERSION 3.10)
project(tessa_audio VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)
find_package(PortAudio REQUIRED)

# Include nlohmann/json as a header-only library
include(FetchContent)
FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
)
FetchContent_MakeAvailable(json)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${ZMQ_INCLUDE_DIRS}
    ${PORTAUDIO_INCLUDE_DIRS}
)

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
    ${ZMQ_LIBRARIES}
    ${PORTAUDIO_LIBRARIES}
    nlohmann_json::nlohmann_json
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