include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
FetchContent_MakeAvailable(googletest)

# Enable testing
enable_testing()

# Set up the test executable
add_executable(tessa_audio_tests
  zmq_connectivity_test.cpp
  device_listing_test.cpp
)

# Link against gtest & project libraries
target_link_libraries(tessa_audio_tests
  GTest::gtest_main
  GTest::gmock_main
  tessa_audio_lib
  nlohmann_json::nlohmann_json
  ${ZeroMQ_LIBRARIES}
  ${PORTAUDIO_LIBRARIES}
  pthread
)

# Add tests to CTest
include(GoogleTest)
gtest_discover_tests(tessa_audio_tests) 