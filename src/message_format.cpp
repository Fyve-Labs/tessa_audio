#include "message_format.hpp"

namespace message_format {

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

json BaseMessage::toJson() const {
    json j;
    j["message_type"] = messageTypeToString(message_type);
    j["timestamp"] = timestamp;
    j["service"] = service;
    if (stream_id) {
        j["stream_id"] = *stream_id;
    }
    return j;
}

BaseMessage BaseMessage::fromJson(const json& j) {
    BaseMessage msg;
    msg.message_type = stringToMessageType(j["message_type"]);
    msg.timestamp = j["timestamp"];
    msg.service = j["service"];
    if (j.contains("stream_id") && !j["stream_id"].is_null()) {
        msg.stream_id = j["stream_id"];
    }
    return msg;
}

json DataMessage::toJson() const {
    json j = BaseMessage::toJson();
    // Payload is binary, so we'll handle it separately
    if (metadata) {
        j["metadata"] = *metadata;
    }
    return j;
}

DataMessage DataMessage::fromJson(const json& j, const std::vector<uint8_t>& binPayload) {
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

json StatusMessage::toJson() const {
    json j = BaseMessage::toJson();
    j["status"] = status;
    return j;
}

StatusMessage StatusMessage::fromJson(const json& j) {
    StatusMessage msg;
    BaseMessage base = BaseMessage::fromJson(j);
    msg.message_type = base.message_type;
    msg.timestamp = base.timestamp;
    msg.service = base.service;
    msg.stream_id = base.stream_id;
    
    msg.status = j["status"].get<std::map<std::string, json>>();
    
    return msg;
}

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    char buf[30];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", std::gmtime(&time_t_now));
    
    // Add milliseconds
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << buf << "." << std::setfill('0') << std::setw(3) << ms.count() << "Z";
    
    return ss.str();
}

} // namespace message_format 