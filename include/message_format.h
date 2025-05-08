#ifndef MESSAGE_FORMAT_H
#define MESSAGE_FORMAT_H

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <ctime>
#include <optional>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace message_format {

enum class MessageType {
    DATA,
    STATUS,
    COMMAND,
    RESPONSE,
    HEARTBEAT
};

std::string messageTypeToString(MessageType type) {
    switch (type) {
        case MessageType::DATA: return "data";
        case MessageType::STATUS: return "status";
        case MessageType::COMMAND: return "command";
        case MessageType::RESPONSE: return "response";
        case MessageType::HEARTBEAT: return "heartbeat";
        default: return "unknown";
    }
}

MessageType stringToMessageType(const std::string& type) {
    if (type == "data") return MessageType::DATA;
    if (type == "status") return MessageType::STATUS;
    if (type == "command") return MessageType::COMMAND;
    if (type == "response") return MessageType::RESPONSE;
    if (type == "heartbeat") return MessageType::HEARTBEAT;
    return MessageType::DATA; // Default
}

// Base message structure
struct BaseMessage {
    MessageType message_type;
    std::string timestamp;
    std::string service;
    std::optional<std::string> stream_id;

    json toJson() const {
        json j;
        j["message_type"] = messageTypeToString(message_type);
        j["timestamp"] = timestamp;
        j["service"] = service;
        if (stream_id) {
            j["stream_id"] = *stream_id;
        }
        return j;
    }

    static BaseMessage fromJson(const json& j) {
        BaseMessage msg;
        msg.message_type = stringToMessageType(j["message_type"]);
        msg.timestamp = j["timestamp"];
        msg.service = j["service"];
        if (j.contains("stream_id") && !j["stream_id"].is_null()) {
            msg.stream_id = j["stream_id"];
        }
        return msg;
    }
};

// Data message structure
struct DataMessage : BaseMessage {
    std::vector<uint8_t> payload;
    std::optional<std::map<std::string, json>> metadata;

    json toJson() const {
        json j = BaseMessage::toJson();
        // Payload is binary, so we'll handle it separately
        if (metadata) {
            j["metadata"] = *metadata;
        }
        return j;
    }

    static DataMessage fromJson(const json& j, const std::vector<uint8_t>& binPayload) {
        DataMessage msg;
        BaseMessage base = BaseMessage::fromJson(j);
        msg.message_type = base.message_type;
        msg.timestamp = base.timestamp;
        msg.service = base.service;
        msg.stream_id = base.stream_id;
        
        msg.payload = binPayload;
        
        if (j.contains("metadata") && !j["metadata"].is_null()) {
            msg.metadata = j["metadata"].get<std::map<std::string, json>>();
        }
        
        return msg;
    }
};

// Status message structure
struct StatusMessage : BaseMessage {
    std::map<std::string, json> status;

    json toJson() const {
        json j = BaseMessage::toJson();
        j["status"] = status;
        return j;
    }

    static StatusMessage fromJson(const json& j) {
        StatusMessage msg;
        BaseMessage base = BaseMessage::fromJson(j);
        msg.message_type = base.message_type;
        msg.timestamp = base.timestamp;
        msg.service = base.service;
        msg.stream_id = base.stream_id;
        
        msg.status = j["status"].get<std::map<std::string, json>>();
        
        return msg;
    }
};

// Helper function to get current timestamp in ISO 8601 format
inline std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    char buf[30];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", std::gmtime(&time_t_now));
    
    // Add milliseconds
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    char timestamp[40];
    std::sprintf(timestamp, "%s.%03ldZ", buf, ms.count());
    
    return std::string(timestamp);
}

} // namespace message_format

#endif // MESSAGE_FORMAT_H 