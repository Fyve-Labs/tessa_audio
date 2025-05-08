#ifndef AUDIO_BUFFER_H
#define AUDIO_BUFFER_H

#include <vector>
#include <mutex>
#include <cstdint>

// AudioBuffer class to handle buffering of audio data
// This helps ensure seamless audio output in case of brief interruptions
class AudioBuffer {
public:
    explicit AudioBuffer(int bufferSizeMs, int sampleRate, int channels, int bitDepth);
    
    // Add new audio data to the buffer
    void addData(const void* data, size_t size, uint64_t timestamp);
    
    // Get buffered data (called by ZMQ publisher)
    std::vector<uint8_t> getData(size_t maxSize, uint64_t& timestamp);
    
    // Clear the buffer
    void clear();
    
    // Get the current buffer size in bytes
    size_t getCurrentSize() const;
    
    // Get the maximum buffer size in bytes
    size_t getMaxSize() const;
    
    // Resize the buffer
    void resize(int bufferSizeMs);
    
private:
    std::vector<uint8_t> buffer_;
    std::mutex bufferMutex_;
    size_t maxSizeBytes_;
    size_t currentPos_;
    uint64_t currentTimestamp_;
    int sampleRate_;
    int channels_;
    int bytesPerSample_;
};

#endif // AUDIO_BUFFER_H 