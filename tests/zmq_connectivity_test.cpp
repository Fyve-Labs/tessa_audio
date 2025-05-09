#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <zmq.hpp>
#include <thread>
#include <chrono>
#include <memory>
#include <atomic>
#include <string>
#include <random>
#include "message_format.hpp"

// Helper function to generate random high ports to avoid conflicts
static int getRandomHighPort() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> distrib(49152, 65535); // Use ephemeral port range
    return distrib(gen);
}

// Test fixture for ZMQ connectivity tests
class ZmqConnectivityTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Generate random high ports to avoid conflicts
        pubPort = getRandomHighPort();
        routerPort = getRandomHighPort();
        
        // Ensure different ports
        while (routerPort == pubPort) {
            routerPort = getRandomHighPort();
        }
        
        pubEndpoint = "tcp://127.0.0.1:" + std::to_string(pubPort);
        routerEndpoint = "tcp://127.0.0.1:" + std::to_string(routerPort);
        
        std::cout << "Using test endpoints: PUB=" << pubEndpoint 
                  << ", ROUTER=" << routerEndpoint << std::endl;
        
        // Create ZMQ context for the test
        context = std::make_unique<zmq::context_t>(1);
        
        // Start server in separate thread
        serverRunning = true;
        serverThread = std::thread(&ZmqConnectivityTest::runTestServer, this);
        
        // Give the server time to start - use a longer timeout for CI environments
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    
    void TearDown() override {
        // Stop the server thread
        serverRunning = false;
        
        // Don't wait more than 5 seconds for thread to join
        auto timeout = std::chrono::seconds(5);
        auto start = std::chrono::steady_clock::now();
        
        if (serverThread.joinable()) {
            while (serverThread.joinable() && 
                  (std::chrono::steady_clock::now() - start < timeout)) {
                try {
                    serverThread.join();
                } catch (const std::exception& e) {
                    std::cerr << "Error joining server thread: " << e.what() << std::endl;
                    break;
                }
            }
        }

        // If we couldn't join the thread, detach it instead of hanging
        if (serverThread.joinable()) {
            std::cerr << "Warning: Server thread didn't terminate within timeout, detaching." << std::endl;
            serverThread.detach();
        }
    }
    
    void runTestServer() {
        try {
            // Create and bind a PUB socket
            zmq::socket_t publisher(*context, zmq::socket_type::pub);
            publisher.bind(pubEndpoint);
            
            // Create and bind a ROUTER socket
            zmq::socket_t router(*context, zmq::socket_type::router);
            router.bind(routerEndpoint);
            
            // Set shorter socket linger period for clean exit
            int linger = 100; // 100ms
            publisher.set(zmq::sockopt::linger, linger);
            router.set(zmq::sockopt::linger, linger);
            
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
                
                try {
                    publisher.send(topicMsg, zmq::send_flags::sndmore);
                    
                    // Send JSON message
                    zmq::message_t jsonMessage(jsonString.size());
                    memcpy(jsonMessage.data(), jsonString.data(), jsonString.size());
                    publisher.send(jsonMessage, zmq::send_flags::none);
                } catch (const zmq::error_t& e) {
                    std::cerr << "ZMQ send error: " << e.what() << std::endl;
                }
                
                // Poll for commands on the router socket (with shorter timeout)
                zmq::poll(&items[0], 1, std::chrono::milliseconds(50));
                
                if (items[0].revents & ZMQ_POLLIN) {
                    try {
                        // Receive client identity
                        zmq::message_t identity;
                        auto ret_val = router.recv(identity);
                        if (!ret_val.has_value()) {
                            std::cerr << "Failed to receive identity" << std::endl;
                            continue;
                        }
                        
                        // Receive empty delimiter
                        zmq::message_t delimiter;
                        ret_val = router.recv(delimiter);
                        if (!ret_val.has_value()) {
                            std::cerr << "Failed to receive delimiter" << std::endl;
                            continue;
                        }
                        
                        // Receive topic
                        zmq::message_t topic;
                        ret_val = router.recv(topic);
                        if (!ret_val.has_value()) {
                            std::cerr << "Failed to receive topic" << std::endl;
                            continue;
                        }
                        
                        // Receive command
                        zmq::message_t command;
                        ret_val = router.recv(command);
                        if (!ret_val.has_value()) {
                            std::cerr << "Failed to receive command" << std::endl;
                            continue;
                        }

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
                        router.send(topic, zmq::send_flags::sndmore);
                        
                        // Send response
                        zmq::message_t responseMsg(response.size());
                        memcpy(responseMsg.data(), response.data(), response.size());
                        router.send(responseMsg, zmq::send_flags::none);
                    } catch (const zmq::error_t& e) {
                        std::cerr << "ZMQ recv/send error: " << e.what() << std::endl;
                    }
                }
                
                // Sleep to avoid busy-waiting (shorter interval)
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            
        } catch (const zmq::error_t& e) {
            std::cerr << "ZMQ error in test server: " << e.what() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error in test server: " << e.what() << std::endl;
        }
    }
    
    std::unique_ptr<zmq::context_t> context;
    std::thread serverThread;
    std::atomic<bool> serverRunning;
    std::string pubEndpoint;
    std::string routerEndpoint;
    int pubPort;
    int routerPort;
};

// Test that we can subscribe and receive messages
TEST_F(ZmqConnectivityTest, CanSubscribeAndReceiveMessages) {
    // Create a subscriber
    zmq::socket_t subscriber(*context, zmq::socket_type::sub);
    subscriber.connect(pubEndpoint);
    subscriber.set(zmq::sockopt::subscribe, "heartbeat");
    
    // Set shorter linger period for clean exit
    int linger = 100; // 100ms
    subscriber.set(zmq::sockopt::linger, linger);
    
    // Poll for messages
    zmq::pollitem_t items[] = {
        { static_cast<void*>(subscriber), 0, ZMQ_POLLIN, 0 }
    };
    
    // Wait for a message with timeout (shorter for test)
    bool messageReceived = false;
    auto startTime = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(3);
    
    while (!messageReceived && 
           (std::chrono::steady_clock::now() - startTime < timeout)) {
        
        // Poll with shorter timeout
        zmq::poll(&items[0], 1, std::chrono::milliseconds(100));
        
        if (items[0].revents & ZMQ_POLLIN) {
            try {
                // Receive topic
                zmq::message_t topicMsg;
                auto ret_val = subscriber.recv(topicMsg);
                if (!ret_val.has_value()) {
                    std::cerr << "Failed to receive topic" << std::endl;
                    continue;
                }
                std::string topic(static_cast<char*>(topicMsg.data()), topicMsg.size());
                
                // Receive JSON message
                zmq::message_t jsonMsg;
                ret_val = subscriber.recv(jsonMsg);
                if (!ret_val.has_value()) {
                    std::cerr << "Failed to receive JSON message" << std::endl;
                    continue;
                }
                std::string jsonString(static_cast<char*>(jsonMsg.data()), jsonMsg.size());
                
                // Check the topic
                EXPECT_EQ(topic, "heartbeat");
                
                // Parse the JSON
                nlohmann::json json = nlohmann::json::parse(jsonString);
                EXPECT_EQ(json["message_type"], "heartbeat");
                EXPECT_EQ(json["service"], "test_service");
                EXPECT_TRUE(json["status"]["server_running"].get<bool>());
                
                messageReceived = true;
            } catch (const zmq::error_t& e) {
                std::cerr << "ZMQ recv error: " << e.what() << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Error receiving message: " << e.what() << std::endl;
            }
        }
    }
    
    EXPECT_TRUE(messageReceived) << "Timed out waiting for message";
}

// Test that we can send commands and get responses
TEST_F(ZmqConnectivityTest, CanSendCommandsAndGetResponses) {
    // Skip if the first test failed - to prevent cascading failures
    if (HasFailure()) {
        GTEST_SKIP() << "Skipping due to previous test failure";
    }
    
    // Create a dealer socket
    zmq::socket_t dealer(*context, zmq::socket_type::dealer);
    dealer.connect(routerEndpoint);
    
    // Set shorter linger period for clean exit
    int linger = 100; // 100ms
    dealer.set(zmq::sockopt::linger, linger);
    
    // Send a PING command
    std::string topic = "control";
    std::string command = "PING";
    
    try {
        // DEALER needs to send an empty delimiter frame for ROUTER compatibility
        zmq::message_t delimiterMsg(0);
        dealer.send(delimiterMsg, zmq::send_flags::sndmore);
        
        // Send topic
        zmq::message_t topicMsg(topic.size());
        memcpy(topicMsg.data(), topic.data(), topic.size());
        dealer.send(topicMsg, zmq::send_flags::sndmore);
        
        // Send command
        zmq::message_t commandMsg(command.size());
        memcpy(commandMsg.data(), command.data(), command.size());
        dealer.send(commandMsg, zmq::send_flags::none);
    } catch (const zmq::error_t& e) {
        FAIL() << "Failed to send command: " << e.what();
        return;
    }
    
    // Poll for response
    zmq::pollitem_t items[] = {
        { static_cast<void*>(dealer), 0, ZMQ_POLLIN, 0 }
    };
    
    // Wait for response with timeout (shorter for test)
    bool responseReceived = false;
    auto startTime = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(3);

    while (!responseReceived && 
           (std::chrono::steady_clock::now() - startTime < timeout)) {
        
        // Poll with shorter timeout
        zmq::poll(&items[0], 1, std::chrono::milliseconds(100));
        
        if (items[0].revents & ZMQ_POLLIN) {
            try {
                // First receive the empty delimiter from ROUTER
                zmq::message_t delimiterMsg;
                auto ret_val = dealer.recv(delimiterMsg);
                if (!ret_val.has_value()) {
                    std::cerr << "Failed to receive delimiter" << std::endl;
                    continue;
                }
                
                // Receive topic
                zmq::message_t topicMsg;
                ret_val = dealer.recv(topicMsg);
                if (!ret_val.has_value()) {
                    std::cerr << "Failed to receive topic" << std::endl;
                    continue;
                }
                std::string recvTopic(static_cast<char*>(topicMsg.data()), topicMsg.size());
                
                // Receive response
                zmq::message_t responseMsg;
                ret_val = dealer.recv(responseMsg);
                if (!ret_val.has_value()) {
                    std::cerr << "Failed to receive response" << std::endl;
                    continue;
                }
                std::string response(static_cast<char*>(responseMsg.data()), responseMsg.size());
                
                // Check response
                EXPECT_EQ(recvTopic, topic);
                EXPECT_EQ(response, "PONG");
                
                responseReceived = true;
            } catch (const zmq::error_t& e) {
                std::cerr << "ZMQ recv error: " << e.what() << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Error receiving response: " << e.what() << std::endl;
            }
        }
    }
    
    EXPECT_TRUE(responseReceived) << "Timed out waiting for response";
} 