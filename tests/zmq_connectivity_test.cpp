#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <zmq.hpp>
#include <thread>
#include <chrono>
#include <memory>
#include <atomic>
#include <string>
#include "message_format.h"

// Test fixture for ZMQ connectivity tests
class ZmqConnectivityTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create ZMQ context for the test
        context = std::make_unique<zmq::context_t>(1);
        
        // Start server in separate thread
        serverRunning = true;
        serverThread = std::thread(&ZmqConnectivityTest::runTestServer, this);
        
        // Give the server time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    void TearDown() override {
        // Stop the server thread
        serverRunning = false;
        if (serverThread.joinable()) {
            serverThread.join();
        }
    }
    
    void runTestServer() {
        try {
            // Create and bind a PUB socket
            zmq::socket_t publisher(*context, zmq::socket_type::pub);
            publisher.bind("tcp://127.0.0.1:5555");
            
            // Create and bind a ROUTER socket
            zmq::socket_t router(*context, zmq::socket_type::router);
            router.bind("tcp://127.0.0.1:5556");
            
            // Poll items for the router socket
            zmq::pollitem_t items[] = {
                { static_cast<void*>(router), 0, ZMQ_POLLIN, 0 }
            };
            
            // Server loop
            while (serverRunning) {
                // Send a heartbeat on the publisher
                message_format::StatusMessage msg;
                msg.message_type = message_format::MessageType::HEARTBEAT;
                msg.timestamp = message_format::getCurrentTimestamp();
                msg.service = "test_service";
                
                // Add status info
                std::map<std::string, nlohmann::json> status;
                status["server_running"] = true;
                msg.status = status;
                
                // Convert to JSON
                nlohmann::json jsonMsg = msg.toJson();
                std::string jsonString = jsonMsg.dump();
                
                // Send topic frame
                std::string topicStr = "heartbeat";
                zmq::message_t topicMsg(topicStr.size());
                memcpy(topicMsg.data(), topicStr.data(), topicStr.size());
                publisher.send(topicMsg, zmq::send_flags::sndmore);
                
                // Send JSON message
                zmq::message_t jsonMessage(jsonString.size());
                memcpy(jsonMessage.data(), jsonString.data(), jsonString.size());
                publisher.send(jsonMessage, zmq::send_flags::none);
                
                // Poll for commands on the router socket (with timeout)
                zmq::poll(&items[0], 1, 100);
                
                if (items[0].revents & ZMQ_POLLIN) {
                    // Receive client identity
                    zmq::message_t identity;
                    router.recv(identity);
                    
                    // Receive empty delimiter
                    zmq::message_t delimiter;
                    router.recv(delimiter);
                    
                    // Receive topic
                    zmq::message_t topic;
                    router.recv(topic);
                    
                    // Receive command
                    zmq::message_t command;
                    router.recv(command);
                    
                    std::string commandStr(static_cast<char*>(command.data()), command.size());
                    std::string response;
                    
                    if (commandStr == "PING") {
                        response = "PONG";
                    } else {
                        response = "ERROR: Unknown command";
                    }
                    
                    // Send response back
                    router.send(identity, zmq::send_flags::sndmore);
                    router.send(delimiter, zmq::send_flags::sndmore);
                    
                    // Send topic back
                    router.send(topic, zmq::send_flags::sndmore);
                    
                    // Send response
                    zmq::message_t responseMsg(response.size());
                    memcpy(responseMsg.data(), response.data(), response.size());
                    router.send(responseMsg, zmq::send_flags::none);
                }
                
                // Sleep to avoid busy-waiting
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        } catch (const zmq::error_t& e) {
            std::cerr << "ZMQ error in test server: " << e.what() << std::endl;
        }
    }
    
    std::unique_ptr<zmq::context_t> context;
    std::thread serverThread;
    std::atomic<bool> serverRunning;
};

// Test that we can subscribe and receive messages
TEST_F(ZmqConnectivityTest, CanSubscribeAndReceiveMessages) {
    // Create a subscriber
    zmq::socket_t subscriber(*context, zmq::socket_type::sub);
    subscriber.connect("tcp://127.0.0.1:5555");
    subscriber.set(zmq::sockopt::subscribe, "heartbeat");
    
    // Poll for messages
    zmq::pollitem_t items[] = {
        { static_cast<void*>(subscriber), 0, ZMQ_POLLIN, 0 }
    };
    
    // Wait for a message with timeout
    bool messageReceived = false;
    auto startTime = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(5);
    
    while (!messageReceived && 
           (std::chrono::steady_clock::now() - startTime < timeout)) {
        
        zmq::poll(&items[0], 1, 500);
        
        if (items[0].revents & ZMQ_POLLIN) {
            // Receive topic
            zmq::message_t topicMsg;
            subscriber.recv(topicMsg);
            std::string topic(static_cast<char*>(topicMsg.data()), topicMsg.size());
            
            // Receive JSON message
            zmq::message_t jsonMsg;
            subscriber.recv(jsonMsg);
            std::string jsonString(static_cast<char*>(jsonMsg.data()), jsonMsg.size());
            
            // Check the topic
            EXPECT_EQ(topic, "heartbeat");
            
            // Parse the JSON
            nlohmann::json json = nlohmann::json::parse(jsonString);
            EXPECT_EQ(json["message_type"], "heartbeat");
            EXPECT_EQ(json["service"], "test_service");
            EXPECT_TRUE(json["status"]["server_running"].get<bool>());
            
            messageReceived = true;
        }
    }
    
    EXPECT_TRUE(messageReceived) << "Timed out waiting for message";
}

// Test that we can send commands and get responses
TEST_F(ZmqConnectivityTest, CanSendCommandsAndGetResponses) {
    // Create a dealer socket
    zmq::socket_t dealer(*context, zmq::socket_type::dealer);
    dealer.connect("tcp://127.0.0.1:5556");
    
    // Send a PING command
    std::string topic = "control";
    std::string command = "PING";
    
    // Send topic
    zmq::message_t topicMsg(topic.size());
    memcpy(topicMsg.data(), topic.data(), topic.size());
    dealer.send(topicMsg, zmq::send_flags::sndmore);
    
    // Send command
    zmq::message_t commandMsg(command.size());
    memcpy(commandMsg.data(), command.data(), command.size());
    dealer.send(commandMsg, zmq::send_flags::none);
    
    // Poll for response
    zmq::pollitem_t items[] = {
        { static_cast<void*>(dealer), 0, ZMQ_POLLIN, 0 }
    };
    
    // Wait for response with timeout
    bool responseReceived = false;
    auto startTime = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(5);
    
    while (!responseReceived && 
           (std::chrono::steady_clock::now() - startTime < timeout)) {
        
        zmq::poll(&items[0], 1, 500);
        
        if (items[0].revents & ZMQ_POLLIN) {
            // Receive topic
            zmq::message_t topicMsg;
            dealer.recv(topicMsg);
            std::string recvTopic(static_cast<char*>(topicMsg.data()), topicMsg.size());
            
            // Receive response
            zmq::message_t responseMsg;
            dealer.recv(responseMsg);
            std::string response(static_cast<char*>(responseMsg.data()), responseMsg.size());
            
            // Check response
            EXPECT_EQ(recvTopic, topic);
            EXPECT_EQ(response, "PONG");
            
            responseReceived = true;
        }
    }
    
    EXPECT_TRUE(responseReceived) << "Timed out waiting for response";
} 