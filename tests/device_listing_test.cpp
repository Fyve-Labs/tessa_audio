#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <iostream>
#include <vector>
#include <portaudio.h>
#include "device_manager.hpp"

// Test fixture for device listing tests
class DeviceListingTest : public ::testing::Test {
protected:
    void SetUp() override {
        deviceManager = std::make_unique<DeviceManager>();
        ASSERT_TRUE(deviceManager->initialize());
    }
    
    void TearDown() override {
        deviceManager.reset();
    }
    
    std::unique_ptr<DeviceManager> deviceManager;
};

// Test that we can list available devices
TEST_F(DeviceListingTest, CanListDevices) {
    // Get all available input devices
    std::vector<AudioDevice> devices = deviceManager->getInputDevices();
    
    // Output devices to console for manual verification
    std::cout << "\n--- Available Audio Input Devices ---" << std::endl;
    for (const auto& device : devices) {
        std::cout << "[" << device.index << "] " << device.name << std::endl;
        std::cout << "    Max Input Channels: " << device.maxInputChannels << std::endl;
        std::cout << "    Default Sample Rate: " << device.defaultSampleRate << " Hz" << std::endl;
    }
    
    // Try to get the default device
    int defaultDevice = deviceManager->getDefaultInputDevice();
    if (defaultDevice >= 0) {
        AudioDevice defaultDeviceInfo = deviceManager->getDeviceInfo(defaultDevice);
        std::cout << "\nDefault Input Device: [" << defaultDevice << "] " 
                  << defaultDeviceInfo.name << std::endl;
        
        // We should be able to query info about the default device
        EXPECT_TRUE(deviceManager->isValidInputDevice(defaultDevice));
        EXPECT_GT(defaultDeviceInfo.maxInputChannels, 0);
    }
    
    // Assuming most systems will have at least one audio input device
    // but this could fail on systems without any, which is fine for a test
    EXPECT_GT(devices.size(), 0) << "No audio input devices found on this system";
}

// Test device name lookup functionality
TEST_F(DeviceListingTest, CanFindDeviceByName) {
    // Get all available input devices
    std::vector<AudioDevice> devices = deviceManager->getInputDevices();
    
    // Skip test if no devices available
    if (devices.empty()) {
        GTEST_SKIP() << "No audio devices available for testing";
        return;
    }
    
    // Try to find the first device by name
    const AudioDevice& firstDevice = devices.front();
    int foundIndex = deviceManager->getDeviceIndexByName(firstDevice.name);
    
    // We should find the exact device
    EXPECT_EQ(foundIndex, firstDevice.index);
    
    // Test partial name matching (if the name is long enough)
    if (firstDevice.name.length() > 5) {
        std::string partialName = firstDevice.name.substr(0, firstDevice.name.length() / 2);
        int foundByPartialName = deviceManager->getDeviceIndexByName(partialName);
        
        // We should find a device with the partial name
        EXPECT_GE(foundByPartialName, 0);
    }
    
    // Test non-existent device name
    int notFoundIndex = deviceManager->getDeviceIndexByName("ThisDeviceDoesNotExist12345");
    EXPECT_EQ(notFoundIndex, -1);
} 