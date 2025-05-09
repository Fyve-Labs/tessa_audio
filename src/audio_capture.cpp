#include "audio_capture.hpp"
#include "device_manager.hpp"
#include <iostream>
#include <chrono>
#include <cstring>

AudioCapture::AudioCapture(const std::string& deviceName, 
                          int sampleRate, 
                          int channels, 
                          int bitDepth,
                          int bufferSize)
    : deviceName_(deviceName),
      sampleRate_(sampleRate),
      channels_(channels),
      bitDepth_(bitDepth),
      bufferSize_(bufferSize),
      stream_(nullptr),
      isInitialized_(false),
      isRunning_(false) {
    
    bytesPerSample_ = (bitDepth / 8);
    
    // Create audio buffer
    audioBuffer_ = std::make_shared<AudioBuffer>(bufferSize, sampleRate, channels, bitDepth);
}

AudioCapture::~AudioCapture() {
    stop();
    
    if (stream_) {
        Pa_CloseStream(stream_);
        stream_ = nullptr;
    }
    
    Pa_Terminate();
}

bool AudioCapture::initialize() {
    if (isInitialized_) {
        return true;
    }
    
    // Initialize PortAudio
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "PortAudio initialization failed: " << Pa_GetErrorText(err) << std::endl;
        return false;
    }
    
    // Get device index from name
    int deviceIndex;
    DeviceManager deviceManager;
    
    if (deviceName_.empty()) {
        // Use default input device
        deviceIndex = Pa_GetDefaultInputDevice();
        if (deviceIndex == paNoDevice) {
            std::cerr << "No default input device found" << std::endl;
            return false;
        }
    } else {
        // Find device by name
        deviceManager.initialize();
        deviceIndex = deviceManager.getDeviceIndexByName(deviceName_);
        
        if (deviceIndex < 0) {
            std::cerr << "Device not found: " << deviceName_ << std::endl;
            return false;
        }
    }
    
    // Get device info
    const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(deviceIndex);
    if (!deviceInfo) {
        std::cerr << "Failed to get device info" << std::endl;
        return false;
    }
    
    // Set actual device name
    deviceName_ = deviceInfo->name;
    
    // Configure stream parameters
    PaStreamParameters inputParams;
    std::memset(&inputParams, 0, sizeof(inputParams));
    inputParams.device = deviceIndex;
    inputParams.channelCount = channels_;
    
    // Set sample format based on bit depth
    if (bitDepth_ == 16) {
        inputParams.sampleFormat = paInt16;
    } else if (bitDepth_ == 24) {
        inputParams.sampleFormat = paInt24;
    } else if (bitDepth_ == 32) {
        inputParams.sampleFormat = paInt32;
    } else if (bitDepth_ == 8) {
        inputParams.sampleFormat = paInt8;
    } else {
        std::cerr << "Unsupported bit depth: " << bitDepth_ << std::endl;
        return false;
    }
    
    inputParams.suggestedLatency = deviceInfo->defaultLowInputLatency;
    inputParams.hostApiSpecificStreamInfo = nullptr;
    
    // Calculate frames per buffer (buffer size in ms to frames)
    unsigned long framesPerBuffer = (sampleRate_ * bufferSize_) / 1000;
    
    // Open stream
    err = Pa_OpenStream(&stream_,
                       &inputParams,
                       nullptr,  // No output
                       sampleRate_,
                       framesPerBuffer,
                       paClipOff | paDitherOff,  // No clipping or dithering
                       &AudioCapture::paCallback,
                       this);
    
    if (err != paNoError) {
        std::cerr << "Failed to open PortAudio stream: " << Pa_GetErrorText(err) << std::endl;
        return false;
    }
    
    isInitialized_ = true;
    return true;
}

bool AudioCapture::start() {
    if (!isInitialized_ && !initialize()) {
        return false;
    }
    
    if (isRunning_) {
        return true;  // Already running
    }
    
    PaError err = Pa_StartStream(stream_);
    if (err != paNoError) {
        std::cerr << "Failed to start PortAudio stream: " << Pa_GetErrorText(err) << std::endl;
        return false;
    }
    
    isRunning_ = true;
    return true;
}

bool AudioCapture::stop() {
    if (!isRunning_) {
        return true;  // Already stopped
    }
    
    if (stream_) {
        PaError err = Pa_StopStream(stream_);
        if (err != paNoError) {
            std::cerr << "Failed to stop PortAudio stream: " << Pa_GetErrorText(err) << std::endl;
            return false;
        }
    }
    
    isRunning_ = false;
    return true;
}

bool AudioCapture::isRunning() const {
    return isRunning_;
}

bool AudioCapture::setSampleRate(int sampleRate) {
    if (isRunning_) {
        stop();
    }
    
    if (stream_) {
        Pa_CloseStream(stream_);
        stream_ = nullptr;
    }
    
    sampleRate_ = sampleRate;
    isInitialized_ = false;
    
    if (!initialize()) {
        return false;
    }
    
    return true;
}

void AudioCapture::setAudioDataCallback(std::function<void(const std::vector<uint8_t>&, uint64_t)> callback) {
    dataCallback_ = callback;
}

int AudioCapture::paCallback(const void* inputBuffer, void* outputBuffer,
                            unsigned long framesPerBuffer,
                            const PaStreamCallbackTimeInfo* timeInfo,
                            PaStreamCallbackFlags statusFlags,
                            void* userData) {
    AudioCapture* self = static_cast<AudioCapture*>(userData);
    
    // Skip if there's no input buffer
    if (!inputBuffer) {
        return paContinue;
    }
    
    // Calculate buffer size in bytes
    size_t bufferSizeBytes = framesPerBuffer * self->channels_ * (self->bitDepth_ / 8);
    
    // Get current timestamp in milliseconds
    uint64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Add data to buffer
    self->audioBuffer_->addData(inputBuffer, bufferSizeBytes, timestamp);
    
    // If a callback is set, pass the data to it
    if (self->dataCallback_) {
        std::vector<uint8_t> data(static_cast<const uint8_t*>(inputBuffer),
                                  static_cast<const uint8_t*>(inputBuffer) + bufferSizeBytes);
        self->dataCallback_(data, timestamp);
    }
    
    return paContinue;
} 