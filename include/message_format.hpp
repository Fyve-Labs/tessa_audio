#ifndef MESSAGE_FORMAT_H
#define MESSAGE_FORMAT_H

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <ctime>
#include <optional>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>

using json = nlohmann::json;

namespace message_format {

enum class MessageType {
    DATA,
    STATUS,
    COMMAND,
    RESPONSE,
    HEARTBEAT
};

std::string messageTypeToString(MessageType type);
MessageType stringToMessageType(const std::string& type);

// Base message structure
struct BaseMessage {
    MessageType message_type;
    std::string timestamp;
    std::string service;
    std::optional<std::string> stream_id;

    json toJson() const;
    static BaseMessage fromJson(const json& j);
};

// Data message structure
struct DataMessage : BaseMessage {
    std::vector<uint8_t> payload;
    std::optional<std::map<std::string, json>> metadata;

    json toJson() const;
    static DataMessage fromJson(const json& j, const std::vector<uint8_t>& binPayload);
};

// Status message structure
struct StatusMessage : BaseMessage {
    std::map<std::string, json> status;

    json toJson() const;
    static StatusMessage fromJson(const json& j);
};

// Helper function to get current timestamp in ISO 8601 format
std::string getCurrentTimestamp();

} // namespace message_format


// Platform-specific ZeroMQ socket option setting
// - macOS/Darwin: Uses newer 'set' method with sockopt::linger
// - Linux/others: Uses older 'setsockopt' method with ZMQ_LINGER
//
// If you encounter compilation errors, you may need to check ZeroMQ version:
// 1. For ZeroMQ 4.3.1+: Use the 'set' method with zmq::sockopt::linger
// 2. For older ZeroMQ: Use 'setsockopt' with ZMQ_LINGER
// not defined, but would like these --- (ZMQ_VERSION_MAJOR >= 4 && ZMQ_VERSION_MINOR >= 3)
#if defined(__APPLE__) || defined(__MACH__)
    #define ZMQ_SOCKET_LINGER_METHOD 
#else
    #undef ZMQ_SOCKET_LINGER_METHOD
#endif


#endif // MESSAGE_FORMAT_H 