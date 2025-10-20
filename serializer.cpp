#include "serializer.h"
#include <cstring> // For std::memcpy
#include <stdexcept>

// --- String Serialization ---
void serialize_string(std::vector<char>& buffer, size_t& offset, const std::string& value) {
    // Write string length
    uint32_t len = value.length();
    if (offset + sizeof(len) > buffer.size()) {
        throw std::out_of_range("Buffer overflow during string length serialization");
    }
    std::memcpy(buffer.data() + offset, &len, sizeof(len));
    offset += sizeof(len);

    // Write string data
    if (offset + len > buffer.size()) {
        throw std::out_of_range("Buffer overflow during string data serialization");
    }
    std::memcpy(buffer.data() + offset, value.data(), len);
    offset += len;
}

std::string deserialize_string(const std::vector<char>& buffer, size_t& offset) {
    // Read string length
    uint32_t len;
    if (offset + sizeof(len) > buffer.size()) {
        throw std::out_of_range("Buffer underflow during string length deserialization");
    }
    std::memcpy(&len, buffer.data() + offset, sizeof(len));
    offset += sizeof(len);

    // Read string data
    if (offset + len > buffer.size()) {
        throw std::out_of_range("Buffer underflow during string data deserialization");
    }
    std::string value(buffer.data() + offset, len);
    offset += len;
    return value;
}

// --- Integer Serialization ---
void serialize_int(std::vector<char>& buffer, size_t& offset, int32_t value) {
    if (offset + sizeof(value) > buffer.size()) {
        throw std::out_of_range("Buffer overflow during int serialization");
    }
    std::memcpy(buffer.data() + offset, &value, sizeof(value));
    offset += sizeof(value);
}

int32_t deserialize_int(const std::vector<char>& buffer, size_t& offset) {
    int32_t value;
    if (offset + sizeof(value) > buffer.size()) {
        throw std::out_of_range("Buffer underflow during int deserialization");
    }
    std::memcpy(&value, buffer.data() + offset, sizeof(value));
    offset += sizeof(value);
    return value;
}
