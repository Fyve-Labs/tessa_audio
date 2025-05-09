#include "zmq_publisher.h"
#include <iostream>
#include <chrono>
#include <cstring>

ZmqPublisher::ZmqPublisher(const std::string& address, 
                         const std::string& topic,
                         std::shared_ptr<AudioBuffer> audioBuffer,
                         std::shared_ptr<AudioCapture> audioCapture,
                         const std::string& serviceName,
                         const std::string& streamId)
    : address_(address),
      topic_(topic),
      serviceName_(serviceName),
      streamId_(streamId),
      audioBuffer_(audioBuffer),
      audioCapture_(audioCapture),
      running_(false),
      initialized_(false) {
}

ZmqPublisher::~ZmqPublisher() {
    stop();
}

bool ZmqPublisher::initialize() {
    if (initialized_) {
        return true;
    }
    
    try {
        // Create ZMQ context and socket
        context_ = std::make_unique<zmq::context_t>(1);
        pubSocket_ = std::make_unique<zmq::socket_t>(*context_, ZMQ_PUB);
        
        // Set linger period to 0 for clean exit
        pubSocket_->set(zmq::sockopt::linger, 0);
        
        // Bind socket to address
        pubSocket_->bind(address_);
        
        initialized_ = true;
        return true;
    } catch (const zmq::error_t& e) {
        std::cerr << "ZMQ initialization error: " << e.what() << std::endl;
        return false;
    }
}

bool ZmqPublisher::start() {
    if (!initialized_ && !initialize()) {
        return false;
    }
    
    if (running_) {
        return true;  // Already running
    }
    
    running_ = true;
    
    // Start publisher thread
    publishThread_ = std::thread(&ZmqPublisher::publishLoop, this);
    
    return true;
}

bool ZmqPublisher::stop() {
    if (!running_) {
        return true;  // Already stopped
    }
    
    running_ = false;
    
    // Wait for thread to finish
    if (publishThread_.joinable()) {
        publishThread_.join();
    }
    
    return true;
}

bool ZmqPublisher::isRunning() const {
    return running_;
}

void ZmqPublisher::publishAudioData(const std::vector<uint8_t>& data, uint64_t timestamp) {
    if (!running_ || !initialized_) {
        return;
    }
    
    try {
        // Create a DataMessage
        message_format::DataMessage msg;
        msg.message_type = message_format::MessageType::DATA;
        msg.timestamp = message_format::getCurrentTimestamp();
        msg.service = serviceName_;
        if (!streamId_.empty()) {
            msg.stream_id = streamId_;
        }
        msg.payload = data;
        
        // Add audio metadata
        std::map<std::string, nlohmann::json> metadata;
        metadata["unix_timestamp_ms"] = timestamp;
        metadata["sample_rate"] = audioCapture_->getSampleRate();
        metadata["channels"] = audioCapture_->getChannels();
        metadata["bit_depth"] = audioCapture_->getBitDepth();
        msg.metadata = metadata;
        
        // Convert to JSON
        nlohmann::json jsonMsg = msg.toJson();
        std::string jsonString = jsonMsg.dump();
        
        // Send topic frame
        zmq::message_t topicMsg(topic_.size());
        memcpy(topicMsg.data(), topic_.data(), topic_.size());
        pubSocket_->send(topicMsg, zmq::send_flags::sndmore);
        
        // Send JSON message
        zmq::message_t jsonMessage(jsonString.size());
        memcpy(jsonMessage.data(), jsonString.data(), jsonString.size());
        pubSocket_->send(jsonMessage, zmq::send_flags::sndmore);
        
        // Send binary payload
        zmq::message_t dataMsg(data.size());
        memcpy(dataMsg.data(), data.data(), data.size());
        pubSocket_->send(dataMsg, zmq::send_flags::none);
        
    } catch (const zmq::error_t& e) {
        std::cerr << "ZMQ send error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error sending audio data: " << e.what() << std::endl;
    }
}

void ZmqPublisher::publishStatusMessage(const std::map<std::string, nlohmann::json>& status, bool echo) {
    if (!initialized_) {
        if (!initialize()) {
            return;
        }
    }
    
    try {
        // Create a StatusMessage
        message_format::StatusMessage msg;
        msg.message_type = message_format::MessageType::STATUS;
        msg.timestamp = message_format::getCurrentTimestamp();
        msg.service = serviceName_;
        if (!streamId_.empty()) {
            msg.stream_id = streamId_;
        }
        msg.status = status;
        
        // Convert to JSON
        nlohmann::json jsonMsg = msg.toJson();
        std::string jsonString = jsonMsg.dump();
        
        // Echo to stdout if requested
        if (echo) {
            std::cout << "Status: " << jsonString << std::endl;
        }
        
        // Send topic frame
        zmq::message_t topicMsg(topic_.size());
        memcpy(topicMsg.data(), topic_.data(), topic_.size());
        pubSocket_->send(topicMsg, zmq::send_flags::sndmore);
        
        // Send JSON message
        zmq::message_t jsonMessage(jsonString.size());
        memcpy(jsonMessage.data(), jsonString.data(), jsonString.size());
        pubSocket_->send(jsonMessage, zmq::send_flags::none);
        
    } catch (const zmq::error_t& e) {
        std::cerr << "ZMQ send error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error sending status message: " << e.what() << std::endl;
    }
}

void ZmqPublisher::publishLoop() {
    // Buffer size to read at once (10ms of audio data)
    const size_t bufferSize = audioBuffer_->getMaxSize() / 10;
    
    while (running_) {
        try {
            // Get data from audio buffer
            uint64_t timestamp;
            std::vector<uint8_t> data = audioBuffer_->getData(bufferSize, timestamp);
            
            if (!data.empty()) {
                publishAudioData(data, timestamp);
            }
            
        } catch (const zmq::error_t& e) {
            std::cerr << "ZMQ error in publish loop: " << e.what() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error in publish loop: " << e.what() << std::endl;
        }
        
        // Sleep for a short time to avoid busy-wait
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
} 