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

#endif // MESSAGE_FORMAT_H 