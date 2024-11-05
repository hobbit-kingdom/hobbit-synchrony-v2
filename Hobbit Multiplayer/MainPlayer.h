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

#include "Utility.h"
#include "../ServerClient/Client.h"
#include "../HobbitGameManager/HobbitGameManager.h"
#include "../HobbitGameManager/NPC.h"

class MainPlayer {
    HobbitProcessAnalyzer* hobbitProcessAnalyzer;
    uint32_t animation;
    Vector3 position, rotation;

    uint32_t level;
    uint32_t newLevel;

    uint32_t bilboPosXPTR;
    uint32_t bilboAnimPTR;
    const uint32_t X_POSITION_PTR = 0x0075BA3C;
    std::atomic<bool> processPackets;

public:
    void readPlayer(std::queue<uint8_t>& gameData)
    {
        newLevel = convertQueueToType<uint32_t>(gameData);
    }
    std::queue<uint8_t> write() {
        // label - Snap, 0 - reserve for size
        std::vector<uint8_t> dataVec = { static_cast<uint8_t>(DataLabel::CONNECTED_PLAYER_SNAP), 0 };
        std::queue<uint8_t> dataQueue;

        // Prepares packets to send
        position.x = hobbitProcessAnalyzer->readData<float>(0x7C4 + bilboPosXPTR, 4);
        position.y = hobbitProcessAnalyzer->readData<float>(0x7C8 + bilboPosXPTR, 4);
        position.z = hobbitProcessAnalyzer->readData<float>(0x7CC + bilboPosXPTR, 4);
        rotation.y = hobbitProcessAnalyzer->readData<float>(0x7AC + bilboPosXPTR, 4);
        uint32_t animBilbo = hobbitProcessAnalyzer->readData<uint32_t>(bilboAnimPTR, 4);

        pushTypeToVector(animBilbo, dataVec);
        dataVec[1] += sizeof(animBilbo);

        pushTypeToVector(position.x, dataVec);
        pushTypeToVector(position.y, dataVec);
        pushTypeToVector(position.z, dataVec);
        dataVec[1] += sizeof(position);

        pushTypeToVector(rotation.y, dataVec);
        dataVec[1] += sizeof(rotation.y);

        // Convert vector to queue
        for (const int& element : dataVec) {
            dataQueue.push(element);
        }

        std::cout << "Sending: X " << position.x << ", Y " << position.y << ", Z " << position.z << ", R" << rotation.y << ", A " << animBilbo << std::endl;
        return dataQueue;
    }

    void setHobbitProcessAnalyzer(HobbitProcessAnalyzer &newHobbitProcessAnalyzer)
    {
        hobbitProcessAnalyzer = &newHobbitProcessAnalyzer;
    }
    void readPtrs() {
        bilboPosXPTR = hobbitProcessAnalyzer->readData<uint32_t>(X_POSITION_PTR, 4);
        bilboAnimPTR = 0x8 + hobbitProcessAnalyzer->readData<uint32_t>(0x560 + hobbitProcessAnalyzer->readData<uint32_t>(X_POSITION_PTR, 4), 4);
    }
};
