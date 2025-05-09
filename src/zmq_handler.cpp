#include "zmq_handler.h"
#include "device_manager.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <cstring>

ZmqHandler::ZmqHandler(const std::string& address, 
                      const std::string& topic,
                      std::shared_ptr<AudioCapture> audioCapture,
                      std::shared_ptr<ZmqPublisher> zmqPublisher)
    : address_(address),
      topic_(topic),
      audioCapture_(audioCapture),
      zmqPublisher_(zmqPublisher),
      running_(false),
      initialized_(false),
      verboseMode_(false) {
    
    // Set up command handlers
    commandHandlers_["STATUS"] = [this](const std::string&) { return handleStatus(); };
    commandHandlers_["SET_SAMPLE_RATE"] = [this](const std::string& args) { return handleSetSampleRate(args); };
    commandHandlers_["STOP"] = [this](const std::string&) { return handleStop(); };
    commandHandlers_["START"] = [this](const std::string&) { return handleStart(); };
    commandHandlers_["GET_DEVICES"] = [this](const std::string&) { return handleGetDevices(); };
    commandHandlers_["SET_VERBOSE"] = [this](const std::string& args) { return handleSetVerbose(args); };
}

ZmqHandler::~ZmqHandler() {
    stop();
}

bool ZmqHandler::initialize() {
    if (initialized_) {
        return true;
    }
    
    try {
        // Create ZMQ context and socket
        context_ = std::make_unique<zmq::context_t>(1);
        dealerSocket_ = std::make_unique<zmq::socket_t>(*context_, ZMQ_ROUTER);
        
        // Set linger period to 0 for clean exit
        dealerSocket_->set(zmq::sockopt::linger, 0);
        
        // Bind socket to address
        dealerSocket_->bind(address_);
        
        initialized_ = true;
        return true;
    } catch (const zmq::error_t& e) {
        std::cerr << "ZMQ initialization error: " << e.what() << std::endl;
        return false;
    }
}

bool ZmqHandler::start() {
    if (!initialized_ && !initialize()) {
        return false;
    }
    
    if (running_) {
        return true;  // Already running
    }
    
    running_ = true;
    
    // Start handler thread
    handleThread_ = std::thread(&ZmqHandler::handleLoop, this);
    
    return true;
}

bool ZmqHandler::stop() {
    if (!running_) {
        return true;  // Already stopped
    }
    
    running_ = false;
    
    // Wait for thread to finish
    if (handleThread_.joinable()) {
        handleThread_.join();
    }
    
    return true;
}

bool ZmqHandler::isRunning() const {
    return running_;
}

void ZmqHandler::handleLoop() {
    std::vector<zmq::pollitem_t> pollItems = {
        { static_cast<void*>(*dealerSocket_), 0, ZMQ_POLLIN, 0 }
    };
    
    while (running_) {
        try {
            // Poll with timeout to allow checking running_ flag
            zmq::poll(pollItems.data(), pollItems.size(), std::chrono::milliseconds(100));
            
            if (pollItems[0].revents & ZMQ_POLLIN) {
                // Receive client identity frame
                zmq::message_t identityMsg;
                auto ret_val = dealerSocket_->recv(identityMsg);
                if (!ret_val.has_value()) {
                    continue; // Failed to receive identity
                }
                
                // Receive empty delimiter frame
                zmq::message_t delimiterMsg;
                ret_val = dealerSocket_->recv(delimiterMsg);
                if (!ret_val.has_value()) {
                    continue; // Failed to receive delimiter
                }
                
                // Receive topic frame
                zmq::message_t topicMsg;
                ret_val = dealerSocket_->recv(topicMsg);
                if (!ret_val.has_value()) {
                    continue; // Failed to receive topic
                }
                std::string receivedTopic(static_cast<char*>(topicMsg.data()), topicMsg.size());
                
                // Check topic
                if (receivedTopic == topic_) {
                    // Receive command frame
                    zmq::message_t commandMsg;
                    ret_val = dealerSocket_->recv(commandMsg);
                    if (!ret_val.has_value()) {
                        continue; // Failed to receive command
                    }
                    std::string command(static_cast<char*>(commandMsg.data()), commandMsg.size());
                    
                    // Parse command and arguments
                    std::string commandName;
                    std::string arguments;
                    
                    size_t spacePos = command.find(' ');
                    if (spacePos != std::string::npos) {
                        commandName = command.substr(0, spacePos);
                        arguments = command.substr(spacePos + 1);
                    } else {
                        commandName = command;
                    }
                    
                    // Handle command
                    std::string response;
                    auto it = commandHandlers_.find(commandName);
                    if (it != commandHandlers_.end()) {
                        response = it->second(arguments);
                    } else {
                        response = "ERROR: Unknown command";
                    }
                    
                    // Send DEALER response back to client
                    dealerSocket_->send(identityMsg, zmq::send_flags::sndmore);  // Client identity
                    dealerSocket_->send(delimiterMsg, zmq::send_flags::sndmore); // Empty delimiter
                    
                    // Send topic frame
                    zmq::message_t responseTopicMsg(topic_.size());
                    memcpy(responseTopicMsg.data(), topic_.data(), topic_.size());
                    dealerSocket_->send(responseTopicMsg, zmq::send_flags::sndmore);
                    
                    // Send response frame
                    zmq::message_t responseMsg(response.size());
                    memcpy(responseMsg.data(), response.data(), response.size());
                    dealerSocket_->send(responseMsg, zmq::send_flags::none);
                }
            }
        } catch (const zmq::error_t& e) {
            std::cerr << "ZMQ error in handler loop: " << e.what() << std::endl;
        }
    }
}

std::string ZmqHandler::handleStatus() {
    std::map<std::string, nlohmann::json> statusData;
    
    // Fill status data
    statusData["running"] = audioCapture_->isRunning();
    statusData["sample_rate"] = audioCapture_->getSampleRate();
    statusData["channels"] = audioCapture_->getChannels();
    statusData["bit_depth"] = audioCapture_->getBitDepth();
    statusData["device"] = audioCapture_->getDeviceName();
    
    // Publish status message
    zmqPublisher_->publishStatusMessage(statusData, verboseMode_.load());
    
    // Return simple status string for DEALER response
    std::stringstream ss;
    ss << "STATUS: ";
    ss << (audioCapture_->isRunning() ? "RUNNING" : "STOPPED");
    ss << ", SAMPLE_RATE: " << audioCapture_->getSampleRate();
    ss << ", CHANNELS: " << audioCapture_->getChannels();
    ss << ", BIT_DEPTH: " << audioCapture_->getBitDepth();
    ss << ", DEVICE: " << audioCapture_->getDeviceName();
    
    return ss.str();
}

std::string ZmqHandler::handleSetSampleRate(const std::string& args) {
    try {
        int sampleRate = std::stoi(args);
        
        if (sampleRate <= 0) {
            return "ERROR: Invalid sample rate";
        }
        
        if (audioCapture_->setSampleRate(sampleRate)) {
            if (audioCapture_->isRunning()) {
                audioCapture_->start();
            }
            
            // Publish status update
            std::map<std::string, nlohmann::json> statusData;
            statusData["running"] = audioCapture_->isRunning();
            statusData["sample_rate"] = audioCapture_->getSampleRate();
            statusData["channels"] = audioCapture_->getChannels();
            statusData["bit_depth"] = audioCapture_->getBitDepth();
            statusData["device"] = audioCapture_->getDeviceName();
            statusData["event"] = "sample_rate_changed";
            zmqPublisher_->publishStatusMessage(statusData, verboseMode_.load());
            
            return "OK: Sample rate set to " + std::to_string(sampleRate);
        } else {
            return "ERROR: Failed to set sample rate";
        }
    } catch (const std::exception& e) {
        return "ERROR: Invalid sample rate format";
    }
}

std::string ZmqHandler::handleStop() {
    if (audioCapture_->stop()) {
        // Publish status update
        std::map<std::string, nlohmann::json> statusData;
        statusData["running"] = false;
        statusData["sample_rate"] = audioCapture_->getSampleRate();
        statusData["channels"] = audioCapture_->getChannels();
        statusData["bit_depth"] = audioCapture_->getBitDepth();
        statusData["device"] = audioCapture_->getDeviceName();
        statusData["event"] = "stopped";
        zmqPublisher_->publishStatusMessage(statusData, verboseMode_.load());
        
        return "OK: Audio capture stopped";
    } else {
        return "ERROR: Failed to stop audio capture";
    }
}

std::string ZmqHandler::handleStart() {
    if (audioCapture_->start()) {
        // Publish status update
        std::map<std::string, nlohmann::json> statusData;
        statusData["running"] = true;
        statusData["sample_rate"] = audioCapture_->getSampleRate();
        statusData["channels"] = audioCapture_->getChannels();
        statusData["bit_depth"] = audioCapture_->getBitDepth();
        statusData["device"] = audioCapture_->getDeviceName();
        statusData["event"] = "started";
        zmqPublisher_->publishStatusMessage(statusData, verboseMode_.load());
        
        return "OK: Audio capture started";
    } else {
        return "ERROR: Failed to start audio capture";
    }
}

std::string ZmqHandler::handleGetDevices() {
    DeviceManager deviceManager;
    if (!deviceManager.initialize()) {
        return "ERROR: Failed to initialize audio device manager";
    }
    
    std::vector<AudioDevice> devices = deviceManager.getInputDevices();
    
    // Create status message with devices list
    std::map<std::string, nlohmann::json> statusData;
    nlohmann::json devicesList = nlohmann::json::array();
    
    for (const auto& device : devices) {
        nlohmann::json deviceJson;
        deviceJson["id"] = device.index;
        deviceJson["name"] = device.name;
        deviceJson["channels"] = device.maxInputChannels;
        deviceJson["sample_rate"] = device.defaultSampleRate;
        devicesList.push_back(deviceJson);
    }
    
    statusData["devices"] = devicesList;
    statusData["default_device"] = deviceManager.getDefaultInputDevice();
    statusData["event"] = "device_list";
    
    // Publish status message
    zmqPublisher_->publishStatusMessage(statusData, verboseMode_.load());
    
    // Also return a text version for the DEALER socket
    std::stringstream ss;
    ss << "DEVICES: " << devices.size() << "\n";
    
    for (const auto& device : devices) {
        ss << "ID: " << device.index << "\n";
        ss << "NAME: " << device.name << "\n";
        ss << "CHANNELS: " << device.maxInputChannels << "\n";
        ss << "SAMPLE_RATE: " << device.defaultSampleRate << "\n";
        ss << "---\n";
    }
    
    return ss.str();
}


std::string ZmqHandler::handleSetVerbose(const std::string& args) {
    if (args == "on" || args == "true" || args == "1") {
        verboseMode_ = true;
        return "OK: Verbose mode enabled";
    } else if (args == "off" || args == "false" || args == "0") {
        verboseMode_ = false;
        return "OK: Verbose mode disabled";
    } else {
        return "ERROR: Invalid argument. Use 'on'/'off', 'true'/'false', or '1'/'0'";
    }
} 