#include <iostream>
#include <string>
#include <memory>
#include <chrono>
#include <thread>
#include <csignal>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <portaudio.h>
#include <fstream>
#include <algorithm>
#include <cstdlib>

#include "audio_capture.h"
#include "zmq_publisher.h"
#include "zmq_handler.h"
#include "device_manager.h"
#include "message_format.h"

// Global flag for signal handling
volatile std::sig_atomic_t gRunning = 1;

// Signal handler
void signalHandler(int signal) {
    gRunning = 0;
}

// Load .env file
bool loadEnvFile(const std::string& filePath) {
    std::ifstream envFile(filePath);
    if (!envFile.is_open()) {
        std::cerr << "Failed to open .env file: " << filePath << std::endl;
        return false;
    }
    
    std::string line;
    while (std::getline(envFile, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // Find the equals sign
        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) {
            continue;
        }
        
        // Extract key and value
        std::string key = line.substr(0, eqPos);
        std::string value = line.substr(eqPos + 1);
        
        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        // Remove quotes if present
        if (value.size() >= 2 && (
            (value.front() == '"' && value.back() == '"') ||
            (value.front() == '\'' && value.back() == '\'')
        )) {
            value = value.substr(1, value.size() - 2);
        }
        
        // Set environment variable
        #ifdef _WIN32
        _putenv_s(key.c_str(), value.c_str());
        #else
        setenv(key.c_str(), value.c_str(), 1);
        #endif
    }
    
    return true;
}

// Get environment variable
std::string getEnvVar(const std::string& name, const std::string& defaultValue = "") {
    const char* value = std::getenv(name.c_str());
    return value ? std::string(value) : defaultValue;
}

// Convert option name to environment variable name (--pub-address -> PUB_ADDRESS)
std::string optionToEnvVar(const std::string& option) {
    std::string result = option;
    
    // Remove leading dashes
    while (!result.empty() && result[0] == '-') {
        result.erase(0, 1);
    }
    
    // Convert to uppercase and replace dashes with underscores
    std::transform(result.begin(), result.end(), result.begin(), 
        [](unsigned char c) { 
            if (c == '-') return '_'; 
            return std::toupper(c);
        });
    
    return result;
}

// Command line argument parsing
struct Arguments {
    std::string inputDevice;
    std::string pubAddress;
    std::string pubTopic;
    std::string dealerAddress;
    std::string dealerTopic;
    std::string serviceName;
    std::string streamId;
    int sampleRate;
    int channels;
    int bitDepth;
    int bufferSize;
    bool listDevices;
    bool echoStatus;
    std::string envFile;
};

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options]\n"
              << "Options:\n"
              << "  --input-device <device_name>     Audio input device name\n"
              << "  --pub-address <address:port>     ZMQ PUB socket address (e.g., tcp://*:5555)\n"
              << "  --pub-topic <topic>              ZMQ PUB topic (default: audio)\n"
              << "  --dealer-address <address:port>  ZMQ DEALER socket address (e.g., tcp://*:5556)\n"
              << "  --dealer-topic <topic>           ZMQ DEALER topic (default: control)\n"
              << "  --service-name <name>            Service name for messages (default: tessa_audio)\n"
              << "  --stream-id <id>                 Stream ID for messages (optional)\n"
              << "  --sample-rate <rate>             Audio sample rate (default: 44100)\n"
              << "  --channels <number>              Number of audio channels (default: 2)\n"
              << "  --bit-depth <depth>              Audio bit depth (default: 16)\n"
              << "  --buffer-size <size>             Audio buffer size in ms (default: 100)\n"
              << "  --echo-status                    Echo status messages to stdout\n"
              << "  --list-devices                   List available audio devices and exit\n"
              << "  --env-file <file>                Load environment variables from file\n"
              << "  --help                           Show this help message\n"
              << "\nEnvironment variables:\n"
              << "  All options can also be set via environment variables using the\n"
              << "  uppercase version of the option name with dashes replaced by underscores.\n"
              << "  For example, --pub-address can be set with PUB_ADDRESS environment variable.\n"
              << "  Command line options take precedence over environment variables.\n";
}

Arguments parseArguments(int argc, char* argv[]) {
    Arguments args;
    
    // First check if we need to load a .env file
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--env-file") == 0 && i + 1 < argc) {
            args.envFile = argv[++i];
            loadEnvFile(args.envFile);
            break;
        }
    }
    
    // Get default values from environment variables
    args.inputDevice = getEnvVar("INPUT_DEVICE", "");
    args.pubAddress = getEnvVar("PUB_ADDRESS", "");
    args.pubTopic = getEnvVar("PUB_TOPIC", "audio");
    args.dealerAddress = getEnvVar("DEALER_ADDRESS", "");
    args.dealerTopic = getEnvVar("DEALER_TOPIC", "control");
    args.serviceName = getEnvVar("SERVICE_NAME", "tessa_audio");
    args.streamId = getEnvVar("STREAM_ID", "");
    
    // Convert numeric environment variables with fallbacks
    std::string sampleRateStr = getEnvVar("SAMPLE_RATE", "44100");
    std::string channelsStr = getEnvVar("CHANNELS", "2");
    std::string bitDepthStr = getEnvVar("BIT_DEPTH", "16");
    std::string bufferSizeStr = getEnvVar("BUFFER_SIZE", "100");
    
    try {
        args.sampleRate = std::stoi(sampleRateStr);
    } catch (...) {
        args.sampleRate = 44100;
    }
    
    try {
        args.channels = std::stoi(channelsStr);
    } catch (...) {
        args.channels = 2;
    }
    
    try {
        args.bitDepth = std::stoi(bitDepthStr);
    } catch (...) {
        args.bitDepth = 16;
    }
    
    try {
        args.bufferSize = std::stoi(bufferSizeStr);
    } catch (...) {
        args.bufferSize = 100;
    }
    
    // Boolean flags
    args.listDevices = getEnvVar("LIST_DEVICES", "false") == "true";
    args.echoStatus = getEnvVar("ECHO_STATUS", "false") == "true";
    
    // Override with command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--input-device") == 0 && i + 1 < argc) {
            args.inputDevice = argv[++i];
        } else if (strcmp(argv[i], "--pub-address") == 0 && i + 1 < argc) {
            args.pubAddress = argv[++i];
        } else if (strcmp(argv[i], "--pub-topic") == 0 && i + 1 < argc) {
            args.pubTopic = argv[++i];
        } else if (strcmp(argv[i], "--dealer-address") == 0 && i + 1 < argc) {
            args.dealerAddress = argv[++i];
        } else if (strcmp(argv[i], "--dealer-topic") == 0 && i + 1 < argc) {
            args.dealerTopic = argv[++i];
        } else if (strcmp(argv[i], "--service-name") == 0 && i + 1 < argc) {
            args.serviceName = argv[++i];
        } else if (strcmp(argv[i], "--stream-id") == 0 && i + 1 < argc) {
            args.streamId = argv[++i];
        } else if (strcmp(argv[i], "--sample-rate") == 0 && i + 1 < argc) {
            args.sampleRate = std::stoi(argv[++i]);
        } else if (strcmp(argv[i], "--channels") == 0 && i + 1 < argc) {
            args.channels = std::stoi(argv[++i]);
        } else if (strcmp(argv[i], "--bit-depth") == 0 && i + 1 < argc) {
            args.bitDepth = std::stoi(argv[++i]);
        } else if (strcmp(argv[i], "--buffer-size") == 0 && i + 1 < argc) {
            args.bufferSize = std::stoi(argv[++i]);
        } else if (strcmp(argv[i], "--echo-status") == 0) {
            args.echoStatus = true;
        } else if (strcmp(argv[i], "--list-devices") == 0) {
            args.listDevices = true;
        } else if (strcmp(argv[i], "--env-file") == 0) {
            // Already handled above
            i++;
        } else if (strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            exit(0);
        } else {
            std::cerr << "Unknown option: " << argv[i] << std::endl;
            printUsage(argv[0]);
            exit(1);
        }
    }
    
    return args;
}

void listAudioDevices() {
    DeviceManager deviceManager;
    if (!deviceManager.initialize()) {
        std::cerr << "Failed to initialize audio device manager" << std::endl;
        return;
    }
    
    std::vector<AudioDevice> devices = deviceManager.getInputDevices();
    std::cout << "Available audio input devices:" << std::endl;
    
    for (const auto& device : devices) {
        std::cout << "  [" << device.index << "] " << device.name << std::endl;
        std::cout << "      Max input channels: " << device.maxInputChannels << std::endl;
        std::cout << "      Default sample rate: " << device.defaultSampleRate << " Hz" << std::endl;
    }
    
    int defaultDevice = deviceManager.getDefaultInputDevice();
    if (defaultDevice >= 0) {
        std::cout << "\nDefault input device: [" << defaultDevice << "] " 
                  << deviceManager.getDeviceInfo(defaultDevice).name << std::endl;
    }
}

int main(int argc, char* argv[]) {
    Arguments args = parseArguments(argc, argv);
    
    // Register signal handler
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // If --list-devices flag is set, list devices and exit
    if (args.listDevices) {
        listAudioDevices();
        return 0;
    }
    
    // Check required arguments
    if (args.pubAddress.empty()) {
        std::cerr << "Error: --pub-address is required" << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    if (args.dealerAddress.empty()) {
        std::cerr << "Error: --dealer-address is required" << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    // Initialize components
    std::shared_ptr<AudioBuffer> audioBuffer = 
        std::make_shared<AudioBuffer>(args.bufferSize, args.sampleRate, args.channels, args.bitDepth);
    
    std::shared_ptr<AudioCapture> audioCapture = 
        std::make_shared<AudioCapture>(args.inputDevice, args.sampleRate, args.channels, args.bitDepth, args.bufferSize);
    
    std::shared_ptr<ZmqPublisher> zmqPublisher = 
        std::make_shared<ZmqPublisher>(args.pubAddress, args.pubTopic, audioBuffer, args.serviceName, args.streamId);
    
    std::shared_ptr<ZmqHandler> zmqHandler = 
        std::make_shared<ZmqHandler>(args.dealerAddress, args.dealerTopic, audioCapture, zmqPublisher);
    
    // Set echo status flag
    zmqHandler->setEchoStatusMessages(args.echoStatus);
    
    // Initialize components
    if (!audioCapture->initialize()) {
        std::cerr << "Failed to initialize audio capture" << std::endl;
        return 1;
    }
    
    if (!zmqPublisher->initialize()) {
        std::cerr << "Failed to initialize ZMQ publisher" << std::endl;
        return 1;
    }
    
    if (!zmqHandler->initialize()) {
        std::cerr << "Failed to initialize ZMQ handler" << std::endl;
        return 1;
    }
    
    // Set up the callback from AudioCapture to ZmqPublisher
    audioCapture->setAudioDataCallback([zmqPublisher](const std::vector<uint8_t>& data, uint64_t timestamp) {
        zmqPublisher->publishAudioData(data, timestamp);
    });
    
    // Start components
    if (!zmqPublisher->start()) {
        std::cerr << "Failed to start ZMQ publisher" << std::endl;
        return 1;
    }
    
    if (!zmqHandler->start()) {
        std::cerr << "Failed to start ZMQ handler" << std::endl;
        zmqPublisher->stop();
        return 1;
    }
    
    if (!audioCapture->start()) {
        std::cerr << "Failed to start audio capture" << std::endl;
        zmqHandler->stop();
        zmqPublisher->stop();
        return 1;
    }
    
    // Send initial status message
    nlohmann::json statusJson;
    std::map<std::string, nlohmann::json> statusData;
    statusData["running"] = true;
    statusData["sample_rate"] = audioCapture->getSampleRate();
    statusData["channels"] = audioCapture->getChannels();
    statusData["bit_depth"] = audioCapture->getBitDepth();
    statusData["device"] = audioCapture->getDeviceName();
    zmqPublisher->publishStatusMessage(statusData, args.echoStatus);
    
    std::cout << "AudioZMQ started successfully" << std::endl;
    std::cout << "Publishing on " << args.pubAddress << " with topic '" << args.pubTopic << "'" << std::endl;
    std::cout << "Handling requests on " << args.dealerAddress << " with topic '" << args.dealerTopic << "'" << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    
    // Main loop
    while (gRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Clean shutdown
    std::cout << "\nShutting down..." << std::endl;
    
    audioCapture->stop();
    zmqHandler->stop();
    zmqPublisher->stop();
    
    // Send final status message indicating shutdown
    statusData["running"] = false;
    zmqPublisher->publishStatusMessage(statusData, args.echoStatus);
    
    std::cout << "Shutdown complete" << std::endl;
    
    return 0;
} 