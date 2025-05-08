#include "audio_buffer.h"
#include <algorithm>
#include <iostream>

AudioBuffer::AudioBuffer(int bufferSizeMs, int sampleRate, int channels, int bitDepth)
    : currentPos_(0), currentTimestamp_(0), sampleRate_(sampleRate), channels_(channels) {
    
    // Calculate bytes per sample
    bytesPerSample_ = (bitDepth / 8) * channels;
    
    // Calculate buffer size in bytes
    int samplesPerMs = sampleRate / 1000;
    maxSizeBytes_ = bufferSizeMs * samplesPerMs * bytesPerSample_;
    
    // Initialize buffer
    buffer_.resize(maxSizeBytes_, 0);
}

void AudioBuffer::addData(const void* data, size_t size, uint64_t timestamp) {
    std::lock_guard<std::mutex> lock(bufferMutex_);
    
    // If the incoming data is larger than the buffer, only take the most recent part
    if (size > maxSizeBytes_) {
        const uint8_t* dataBytes = static_cast<const uint8_t*>(data);
        size_t offset = size - maxSizeBytes_;
        
        std::copy(dataBytes + offset, dataBytes + size, buffer_.begin());
        currentPos_ = 0;
        currentTimestamp_ = timestamp + (offset / bytesPerSample_ * 1000 / sampleRate_);
    } else {
        // Copy data to buffer, handling wrap-around
        size_t remainingSpace = maxSizeBytes_ - currentPos_;
        const uint8_t* dataBytes = static_cast<const uint8_t*>(data);
        
        if (size <= remainingSpace) {
            // Data fits in remaining space
            std::copy(dataBytes, dataBytes + size, buffer_.begin() + currentPos_);
            currentPos_ = (currentPos_ + size) % maxSizeBytes_;
        } else {
            // Data wraps around buffer
            std::copy(dataBytes, dataBytes + remainingSpace, buffer_.begin() + currentPos_);
            std::copy(dataBytes + remainingSpace, dataBytes + size, buffer_.begin());
            currentPos_ = size - remainingSpace;
        }
        
        // Update timestamp
        currentTimestamp_ = timestamp;
    }
}

std::vector<uint8_t> AudioBuffer::getData(size_t maxSize, uint64_t& timestamp) {
    std::lock_guard<std::mutex> lock(bufferMutex_);
    
    std::vector<uint8_t> result;
    
    if (buffer_.empty()) {
        timestamp = currentTimestamp_;
        return result;
    }
    
    // Limit maxSize to buffer size
    maxSize = std::min(maxSize, buffer_.size());
    
    // Calculate how much data to return
    size_t dataToReturn = std::min(maxSize, buffer_.size());
    
    // Copy data from buffer
    result.resize(dataToReturn);
    
    // If current position is at the beginning, copy from the end of the buffer
    if (currentPos_ == 0) {
        std::copy(buffer_.end() - dataToReturn, buffer_.end(), result.begin());
    } else {
        // Start from current position - dataToReturn
        size_t startPos = (currentPos_ >= dataToReturn) ? currentPos_ - dataToReturn : maxSizeBytes_ - (dataToReturn - currentPos_);
        
        if (startPos + dataToReturn <= maxSizeBytes_) {
            // Data doesn't wrap around buffer
            std::copy(buffer_.begin() + startPos, buffer_.begin() + startPos + dataToReturn, result.begin());
        } else {
            // Data wraps around buffer
            size_t firstChunk = maxSizeBytes_ - startPos;
            std::copy(buffer_.begin() + startPos, buffer_.end(), result.begin());
            std::copy(buffer_.begin(), buffer_.begin() + (dataToReturn - firstChunk), result.begin() + firstChunk);
        }
    }
    
    // Calculate timestamp for this data chunk
    size_t msOffset = (dataToReturn / bytesPerSample_) * 1000 / sampleRate_;
    timestamp = currentTimestamp_ - msOffset;
    
    return result;
}

void AudioBuffer::clear() {
    std::lock_guard<std::mutex> lock(bufferMutex_);
    std::fill(buffer_.begin(), buffer_.end(), 0);
    currentPos_ = 0;
    currentTimestamp_ = 0;
}

size_t AudioBuffer::getCurrentSize() const {
    return buffer_.size();
}

size_t AudioBuffer::getMaxSize() const {
    return maxSizeBytes_;
}

void AudioBuffer::resize(int bufferSizeMs) {
    std::lock_guard<std::mutex> lock(bufferMutex_);
    
    // Calculate new buffer size in bytes
    int samplesPerMs = sampleRate_ / 1000;
    size_t newSizeBytes = bufferSizeMs * samplesPerMs * bytesPerSample_;
    
    // Resize buffer
    buffer_.resize(newSizeBytes, 0);
    maxSizeBytes_ = newSizeBytes;
    
    // Reset position if it's now out of bounds
    if (currentPos_ >= maxSizeBytes_) {
        currentPos_ = 0;
    }
} 