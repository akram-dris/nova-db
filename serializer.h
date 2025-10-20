#ifndef NOVADB_SERIALIZER_H
#define NOVADB_SERIALIZER_H

#include <vector>
#include <string>
#include <cstdint>

// Basic serialization functions
void serialize_string(std::vector<char>& buffer, size_t& offset, const std::string& value);
std::string deserialize_string(const std::vector<char>& buffer, size_t& offset);

void serialize_int(std::vector<char>& buffer, size_t& offset, int32_t value);
int32_t deserialize_int(const std::vector<char>& buffer, size_t& offset);

#endif //NOVADB_SERIALIZER_H
