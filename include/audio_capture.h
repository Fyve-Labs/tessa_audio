#ifndef AUDIO_CAPTURE_H
#define AUDIO_CAPTURE_H

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <portaudio.h>
#include "audio_buffer.h"

class AudioCapture {
public:
    AudioCapture(const std::string& deviceName, 
                 int sampleRate, 
                 int channels, 
                 int bitDepth,
                 int bufferSize);
    ~AudioCapture();

    bool initialize();
    bool start();
    bool stop();
    bool isRunning() const;
    
    // Getters for current settings
    int getSampleRate() const { return sampleRate_; }
    int getChannels() const { return channels_; }
    int getBitDepth() const { return bitDepth_; }
    std::string getDeviceName() const { return deviceName_; }
    
    // Setters that can be called via ZMQ commands
    bool setSampleRate(int sampleRate);
    
    // Set callback for when new audio data is available
    void setAudioDataCallback(std::function<void(const std::vector<uint8_t>&, uint64_t)> callback);
    
private:
    static int paCallback(const void* inputBuffer, void* outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo* timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void* userData);

    std::string deviceName_;
    int sampleRate_;
    int channels_;
    int bitDepth_;
    int bufferSize_;  // in milliseconds
    int bytesPerSample_;
    
    PaStream* stream_;
    bool isInitialized_;
    bool isRunning_;
    
    std::shared_ptr<AudioBuffer> audioBuffer_;
    std::function<void(const std::vector<uint8_t>&, uint64_t)> dataCallback_;
};

#endif // AUDIO_CAPTURE_H 