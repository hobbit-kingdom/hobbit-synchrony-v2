#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint> // For uint32_t

class ChatMessage {
public:
    enum { header_length = sizeof(uint32_t) }; // Use 4 bytes for length
    enum { max_body_length = 65536 }; // Max message size (64k, adjust as needed)

    ChatMessage() : body_length_(0) {}

    const char* data() const {
        return data_;
    }

    char* data() {
        return data_;
    }

    std::size_t length() const {
        return header_length + body_length_;
    }

    const char* body() const {
        return data_ + header_length;
    }

    char* body() {
        return data_ + header_length;
    }

    std::size_t body_length() const {
        return body_length_;
    }

    void body_length(std::size_t new_length) {
        body_length_ = new_length;
        if (body_length_ > max_body_length)
            body_length_ = max_body_length;
    }

    bool decode_header() {
        uint32_t net_length;
        std::memcpy(&net_length, data_, header_length);
        body_length_ = ntohl(net_length); // Convert from network to host byte order
        if (body_length_ > max_body_length) {
            body_length_ = 0;
            return false;
        }
        return true;
    }

    void encode_header() {
        uint32_t net_length = htonl((uint32_t)body_length_); // Convert to network byte order
        std::memcpy(data_, &net_length, header_length);
    }

private:
    char data_[header_length + max_body_length];
    std::size_t body_length_;
};