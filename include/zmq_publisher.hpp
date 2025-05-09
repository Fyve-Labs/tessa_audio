#ifndef ZMQ_PUBLISHER_H
#define ZMQ_PUBLISHER_H

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <zmq.hpp>
#include "audio_buffer.h"
#include "audio_capture.h"
#include "message_format.h"

class ZmqPublisher {
public:
    ZmqPublisher(const std::string& address, 
                 const std::string& topic,
                 std::shared_ptr<AudioBuffer> audioBuffer,
                 std::shared_ptr<AudioCapture> audioCapture,
                 const std::string& serviceName,
                 const std::string& streamId = "");
    ~ZmqPublisher();

    bool initialize();
    bool start();
    bool stop();
    bool isRunning() const;
    
    std::string getAddress() const { return address_; }
    std::string getTopic() const { return topic_; }
    
    // Used by AudioCapture to send new data directly
    void publishAudioData(const std::vector<uint8_t>& data, uint64_t timestamp);

    // Publish a status message
    void publishStatusMessage(const std::map<std::string, nlohmann::json>& status, bool echo = false);
    
private:
    void publishLoop();
    
    std::string address_;
    std::string topic_;
    std::string serviceName_;
    std::string streamId_;
    
    std::unique_ptr<zmq::context_t> context_;
    std::unique_ptr<zmq::socket_t> pubSocket_;
    
    std::shared_ptr<AudioBuffer> audioBuffer_;
    std::shared_ptr<AudioCapture> audioCapture_;
    std::thread publishThread_;
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;
};

#endif // ZMQ_PUBLISHER_H 