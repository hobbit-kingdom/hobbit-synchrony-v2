#pragma once
#include <string>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <queue>
#include <vector>
#include <vector>
#include <cstdint>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <iomanip>

#include "../ServerClient/Client.h"

// Utility functions for handling serialization and deserialization
template <typename T>
T convertQueueToType(std::queue<uint8_t>& myQueue) {
    if (myQueue.size() < sizeof(T)) {
        throw std::runtime_error("Not enough elements in the queue");
    }

    T result;
    std::vector<uint8_t> buffer(sizeof(T));
    for (size_t i = 0; i < sizeof(T); ++i) {
        buffer[i] = myQueue.front();
        myQueue.pop();
    }

    std::memcpy(&result, buffer.data(), sizeof(T));
    return result;
}

template <typename T>
void pushTypeToVector(const T& value, std::vector<uint8_t>& myVector) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
    myVector.insert(myVector.end(), bytes, bytes + sizeof(T));
}

// Enum for data labels
enum class DataLabel {
    SERVER = 0,
    CONNECTED_PLAYER_SNAP = 1,
    CONNECTED_PLAYER_LEVEL = 2
};

// Structs for message handling
struct MessageBundle {
    BaseMessage* textResponse = nullptr;
    BaseMessage* eventResponse = nullptr;
    BaseMessage* snapshotResponse = nullptr;
};

struct Vector3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;

    Vector3() = default;
    Vector3(const Vector3& vec) : x(vec.x), y(vec.y), z(vec.z) {}

    Vector3& operator=(const Vector3& vec) {
        x = vec.x; y = vec.y; z = vec.z;
        return *this;
    }

    Vector3& operator+=(const Vector3& vec) {
        x += vec.x; y += vec.y; z += vec.z;
        return *this;
    }

    Vector3& operator-=(const Vector3& vec) {
        x -= vec.x; y -= vec.y; z -= vec.z;
        return *this;
    }
};
