#ifndef ZMQ_HANDLER_H
#define ZMQ_HANDLER_H

#include <string>
#include <thread>
#include <atomic>
#include <memory>
#include <functional>
#include <unordered_map>
#include <zmq.hpp>
#include "audio_capture.h"
#include "zmq_publisher.h"
#include "message_format.h"

class ZmqHandler {
public:
    ZmqHandler(const std::string& address, 
               const std::string& topic,
               std::shared_ptr<AudioCapture> audioCapture,
               std::shared_ptr<ZmqPublisher> zmqPublisher);
    ~ZmqHandler();

    bool initialize();
    bool start();
    bool stop();
    bool isRunning() const;
    
    // Flag to control whether to echo status messages to stdout
    void setVerboseMode(bool verbose) { verboseMode_ = verbose; }
    bool getVerboseMode() const { return verboseMode_; }
    
    // Getters
    std::string getAddress() const { return address_; }
    std::string getTopic() const { return topic_; }
    
private:
    void handleLoop();
    
    // Command handlers
    std::string handleStatus();
    std::string handleSetSampleRate(const std::string& args);
    std::string handleStop();
    std::string handleStart();
    std::string handleGetDevices();
    std::string handleSetVerbose(const std::string& args);
    
    std::string address_;
    std::string topic_;
    
    std::unique_ptr<zmq::context_t> context_;
    std::unique_ptr<zmq::socket_t> dealerSocket_;
    
    std::shared_ptr<AudioCapture> audioCapture_;
    std::shared_ptr<ZmqPublisher> zmqPublisher_;
    
    std::thread handleThread_;
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;
    std::atomic<bool> verboseMode_;
    
    // Map of command strings to handler functions
    std::unordered_map<
        std::string, 
        std::function<std::string(const std::string&)>
    > commandHandlers_;
};

#endif // ZMQ_HANDLER_H 