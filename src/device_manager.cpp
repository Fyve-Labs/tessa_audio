#include "device_manager.h"
#include <iostream>
#include <algorithm>

DeviceManager::DeviceManager() : initialized_(false) {}

DeviceManager::~DeviceManager() {}

bool DeviceManager::initialize() {
    if (initialized_) {
        return true;
    }
    
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "PortAudio initialization failed: " << Pa_GetErrorText(err) << std::endl;
        return false;
    }
    
    initialized_ = true;
    return true;
}

std::vector<AudioDevice> DeviceManager::getInputDevices() {
    std::vector<AudioDevice> devices;
    
    if (!initialized_ && !initialize()) {
        return devices;
    }
    
    int numDevices = Pa_GetDeviceCount();
    if (numDevices < 0) {
        std::cerr << "PortAudio error: " << Pa_GetErrorText(numDevices) << std::endl;
        return devices;
    }
    
    for (int i = 0; i < numDevices; i++) {
        const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
        
        if (deviceInfo && deviceInfo->maxInputChannels > 0) {
            AudioDevice device;
            device.index = i;
            device.name = deviceInfo->name;
            device.maxInputChannels = deviceInfo->maxInputChannels;
            device.maxOutputChannels = deviceInfo->maxOutputChannels;
            device.defaultSampleRate = deviceInfo->defaultSampleRate;
            
            devices.push_back(device);
        }
    }
    
    return devices;
}

int DeviceManager::getDeviceIndexByName(const std::string& name) {
    if (!initialized_ && !initialize()) {
        return -1;
    }
    
    int numDevices = Pa_GetDeviceCount();
    if (numDevices < 0) {
        std::cerr << "PortAudio error: " << Pa_GetErrorText(numDevices) << std::endl;
        return -1;
    }
    
    for (int i = 0; i < numDevices; i++) {
        const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
        
        if (deviceInfo && deviceInfo->maxInputChannels > 0) {
            if (name == deviceInfo->name) {
                return i;
            }
        }
    }
    
    // If no exact match, try partial match (case-insensitive)
    for (int i = 0; i < numDevices; i++) {
        const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
        
        if (deviceInfo && deviceInfo->maxInputChannels > 0) {
            std::string deviceName = deviceInfo->name;
            std::transform(deviceName.begin(), deviceName.end(), deviceName.begin(), 
                          [](unsigned char c){ return std::tolower(c); });
            
            std::string searchName = name;
            std::transform(searchName.begin(), searchName.end(), searchName.begin(),
                          [](unsigned char c){ return std::tolower(c); });
            
            if (deviceName.find(searchName) != std::string::npos) {
                return i;
            }
        }
    }
    
    return -1;
}

int DeviceManager::getDefaultInputDevice() {
    if (!initialized_ && !initialize()) {
        return -1;
    }
    
    return Pa_GetDefaultInputDevice();
}

bool DeviceManager::isValidInputDevice(int deviceIndex) {
    if (!initialized_ && !initialize()) {
        return false;
    }
    
    int numDevices = Pa_GetDeviceCount();
    if (deviceIndex < 0 || deviceIndex >= numDevices) {
        return false;
    }
    
    const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(deviceIndex);
    return (deviceInfo && deviceInfo->maxInputChannels > 0);
}

AudioDevice DeviceManager::getDeviceInfo(int deviceIndex) {
    AudioDevice device;
    device.index = -1;
    
    if (!initialized_ && !initialize()) {
        return device;
    }
    
    if (deviceIndex < 0) {
        return device;
    }
    
    const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(deviceIndex);
    if (!deviceInfo) {
        return device;
    }
    
    device.index = deviceIndex;
    device.name = deviceInfo->name;
    device.maxInputChannels = deviceInfo->maxInputChannels;
    device.maxOutputChannels = deviceInfo->maxOutputChannels;
    device.defaultSampleRate = deviceInfo->defaultSampleRate;
    
    return device;
} 