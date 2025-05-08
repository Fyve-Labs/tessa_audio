# Set up googletest directly instead of using FetchContent
set(GOOGLETEST_DIR "${CMAKE_CURRENT_BINARY_DIR}/googletest")
set(GOOGLETEST_INCLUDE_DIR "${GOOGLETEST_DIR}/include")
set(GOOGLETEST_BIN_DIR "${GOOGLETEST_DIR}/bin")
set(GOOGLETEST_LIB_DIR "${GOOGLETEST_DIR}/lib")

# Download and build googletest if needed
if(NOT EXISTS "${GOOGLETEST_INCLUDE_DIR}/gtest/gtest.h")
    message(STATUS "Downloading GoogleTest...")
    
    # Create directories
    file(MAKE_DIRECTORY ${GOOGLETEST_DIR} ${GOOGLETEST_BIN_DIR} ${GOOGLETEST_LIB_DIR})
    
    # Download and extract googletest
    file(DOWNLOAD
        https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        "${GOOGLETEST_DIR}/googletest.zip"
        SHOW_PROGRESS
    )
    
    # Extract the archive
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xf googletest.zip
        WORKING_DIRECTORY ${GOOGLETEST_DIR}
    )
    
    # Build googletest
    execute_process(
        COMMAND ${CMAKE_COMMAND} -S "${GOOGLETEST_DIR}/googletest-1.14.0" -B "${GOOGLETEST_DIR}/build" 
                -DCMAKE_INSTALL_PREFIX=${GOOGLETEST_DIR}
        RESULT_VARIABLE result
    )
    
    execute_process(
        COMMAND ${CMAKE_COMMAND} --build "${GOOGLETEST_DIR}/build" --target install
        RESULT_VARIABLE result
    )
endif()

# Create interface targets for googletest
add_library(GTest::gtest INTERFACE IMPORTED)
set_target_properties(GTest::gtest PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${GOOGLETEST_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES "${GOOGLETEST_LIB_DIR}/libgtest.a")

add_library(GTest::gtest_main INTERFACE IMPORTED)
set_target_properties(GTest::gtest_main PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${GOOGLETEST_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES "${GOOGLETEST_LIB_DIR}/libgtest_main.a")

add_library(GTest::gmock INTERFACE IMPORTED)
set_target_properties(GTest::gmock PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${GOOGLETEST_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES "${GOOGLETEST_LIB_DIR}/libgmock.a")

add_library(GTest::gmock_main INTERFACE IMPORTED)
set_target_properties(GTest::gmock_main PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${GOOGLETEST_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES "${GOOGLETEST_LIB_DIR}/libgmock_main.a")

# Enable testing
enable_testing()

# Add include directories for tests
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${JSON_INCLUDE_DIR}
    ${ZeroMQ_INCLUDE_DIRS}
    ${PORTAUDIO_INCLUDE_DIRS}
)

# Set up the test executable
add_executable(tessa_audio_tests
  zmq_connectivity_test.cpp
  device_listing_test.cpp
)

# Link against gtest & project libraries
target_link_libraries(tessa_audio_tests
  tessa_audio_lib
  GTest::gtest
  GTest::gmock
  GTest::gtest_main
  GTest::gmock_main
  ${ZeroMQ_LIBRARIES}
  ${PORTAUDIO_LIBRARIES}
  pthread
)

# Add tests to CTest
include(GoogleTest)
gtest_discover_tests(tessa_audio_tests) 