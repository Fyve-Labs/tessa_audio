#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include <string>
#include <vector>
#include <portaudio.h>

struct AudioDevice {
    int index;
    std::string name;
    int maxInputChannels;
    int maxOutputChannels;
    double defaultSampleRate;
};

class DeviceManager {
public:
    DeviceManager();
    ~DeviceManager();
    
    bool initialize();
    
    // List all available audio devices
    std::vector<AudioDevice> getInputDevices();
    
    // Get a device by name
    int getDeviceIndexByName(const std::string& name);
    
    // Get the default input device
    int getDefaultInputDevice();
    
    // Check if a device is valid
    bool isValidInputDevice(int deviceIndex);
    
    // Get device info
    AudioDevice getDeviceInfo(int deviceIndex);
    
private:
    bool initialized_;
};

#endif // DEVICE_MANAGER_H 