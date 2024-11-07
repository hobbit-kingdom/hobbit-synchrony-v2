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
    float animFrame, lastAnimFrame;
    Vector3 position, rotation;

    uint32_t level;

    std::queue<std::pair<uint64_t, float>> hurtEnemies;// pair <guid, Health>
public:
    int id = -1;
    NPC npc;
    void readConectedPlayerSnap(std::queue<uint8_t>& gameData) {
        animation = convertQueueToType<uint32_t>(gameData);
        animFrame = convertQueueToType<float>(gameData);
        lastAnimFrame = convertQueueToType<float>(gameData);
        position.x = convertQueueToType<float>(gameData);
        position.y = convertQueueToType<float>(gameData);
        position.z = convertQueueToType<float>(gameData);
        rotation.y = convertQueueToType<float>(gameData);
    }
    void readProcessEnemiesHealth(std::queue<uint8_t>& gameData) {
    uint32_t numberHurtEnemies = convertQueueToType<uint32_t>(gameData);


        for (int i = 0; i < numberHurtEnemies; ++i)
        {
            uint64_t guid = convertQueueToType<uint64_t>(gameData);
            float health = convertQueueToType<float>(gameData);
            hurtEnemies.push(std::pair(guid, health));
        }
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
        npc.setAnimFrames(animFrame, lastAnimFrame);

        // Display the data
        std::cout << "\033[33m";
        std::cout << "Recieve the packet Send: " << std::endl;
        std::cout << "X: " << position.x << " || ";
        std::cout << "Y: " << position.y << " || ";
        std::cout << "Z: " << position.z << " || ";
        std::cout << "R: " << rotation.y << " || ";
        std::cout << "A: " << animation << std::endl << std::endl;
        std::cout << "\033[0m";

        // Update Health of Enemies
        while (!hurtEnemies.empty())
        {
            if (hurtEnemies.front().first == 0 || hurtEnemies.front().second < 0)
            {
                hurtEnemies.pop();
                continue;
            }
            std::cout << "\033[31mENEMY HURT!!  ";
            std::cout <<std::hex <<hurtEnemies.front().first << std::dec<< " " << hurtEnemies.front().second;
            std::cout << "\033[0m";
            NPC enemy;
            enemy.setNCP(hurtEnemies.front().first);
            if (enemy.getHealth() < hurtEnemies.front().second || enemy.getGUID() == 0)
            {
                hurtEnemies.pop();
                continue;
            }
            
            enemy.setHealth(hurtEnemies.front().second);
            hurtEnemies.pop();

        }
    }


    std::vector<BaseMessage> write() {
        //In case you need to send some state from the npc bilbo (which is unlickely) 
        return std::vector<BaseMessage>();
    }

    void clear() { id = -1; }

    void setHobbitProcessAnalyzer(HobbitProcessAnalyzer &newHobbitProcessAnalyzer)
    {
        hobbitProcessAnalyzer = &newHobbitProcessAnalyzer;
        npc.setHobbitProcessAnalyzer(&newHobbitProcessAnalyzer);
    }
};

