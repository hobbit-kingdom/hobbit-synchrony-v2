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

// Player classes
class ConnectedPlayer {
    HobbitProcessAnalyzer* hobbitProcessAnalyzer;
    uint32_t animation;
    Vector3 position, rotation;

    uint32_t level;
public:
    int id = -1;
    NPC npc;
    void readConectedPlayerSnap(std::queue<uint8_t>& gameData) {
        animation = convertQueueToType<uint32_t>(gameData);
        position.x = convertQueueToType<float>(gameData);
        position.y = convertQueueToType<float>(gameData);
        position.z = convertQueueToType<float>(gameData);
        rotation.y = convertQueueToType<float>(gameData);
    }
    void readConectedPlayerLevel(std::queue<uint8_t>& gameData)
    {
        level = convertQueueToType<uint32_t>(gameData);
        gameData.pop();
    }

    void processPlayer(uint8_t myId)
    {
        if (id == -1 || id == myId)
            return;
        //set position, rotation, and animation
        npc.setPositionX(position.x);
        npc.setPositionY(position.y);
        npc.setPositionZ(position.z);
        npc.setRotationY(rotation.y);
        npc.setAnimation(animation);

        // Display the data
        std::cout << "\033[33m";
        std::cout << "Recieve the packet Send: " << std::endl;
        std::cout << "X: " << position.x << " || ";
        std::cout << "Y: " << position.y << " || ";
        std::cout << "Z: " << position.z << " || ";
        std::cout << "R: " << rotation.y << " || ";
        std::cout << "A: " << animation << std::endl << std::endl;
        std::cout << "\033[0m";
    }
    void clear() { id = -1; }

    void setHobbitProcessAnalyzer(HobbitProcessAnalyzer &newHobbitProcessAnalyzer)
    {
        hobbitProcessAnalyzer = &newHobbitProcessAnalyzer;
    }
};

