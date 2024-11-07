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
    int8_t weapon;

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
        weapon = convertQueueToType<int8_t>(gameData);
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
        if (animation != npc.getAnimation())
        {
            npc.setAnimation(animation);
            npc.setAnimFrames(animFrame, lastAnimFrame);
        }


        if (weapon == -1)
        {
            uint64_t guidNone = 0x0D8AD910E885100D;
            //uint64_t guidNone1 = std::stoull(guidNone, nullptr, 16);
            uint32_t addrsGuidNone = NPC::hobbitProcessAnalyzer->findGameObjByGUID(guidNone);
            npc.setWeapon(NPC::hobbitProcessAnalyzer->readData<uint32_t>(addrsGuidNone + 0x260));
        }
        else if (weapon == 0)
        {
            uint64_t guidSting = 0x0D8AD910E885100B;
            //uint64_t guidSting1 = std::stoull(guidSting, nullptr, 16);
            uint32_t addrsGuidSting = NPC::hobbitProcessAnalyzer->findGameObjByGUID(guidSting);
            npc.setWeapon(NPC::hobbitProcessAnalyzer->readData<uint32_t>(addrsGuidSting + 0x260));
        }
        else if (weapon == 1)
        {
            uint64_t guidStaff = 0x0D8AD910E885100A;
            //uint64_t guidStaff1 = std::stoull(guidStaff, nullptr, 16);
            uint32_t addrsGuidStaff = NPC::hobbitProcessAnalyzer->findGameObjByGUID(guidStaff);
            npc.setWeapon(NPC::hobbitProcessAnalyzer->readData<uint32_t>(addrsGuidStaff + 0x260));
        }
        else if (weapon == 3)
        {
            uint64_t guidStone = 0x0D8AD910E885100C;
            //uint64_t guidStone1 = std::stoull(guidStone, nullptr, 16);
            uint32_t addrsGuidStone = NPC::hobbitProcessAnalyzer->findGameObjByGUID(guidStone);
            npc.setWeapon(NPC::hobbitProcessAnalyzer->readData<uint32_t>(addrsGuidStone + 0x260));
        }

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
            if (hurtEnemies.front().first == 0)
            {
                hurtEnemies.pop();
                continue;
            }
            std::cout << "\033[31mENEMY HURT!!  ";
            std::cout <<std::hex <<hurtEnemies.front().first << std::dec<< " " << hurtEnemies.front().second;
            std::cout << "\033[0m";
            //find by guid
            uint32_t objAddrs = NPC::hobbitProcessAnalyzer->findGameObjByGUID(hurtEnemies.front().first);

            //check if the object exist
            if (objAddrs != 0)
            {
                float health = NPC::hobbitProcessAnalyzer->readData<float>(objAddrs + 0x290);
                if (health > hurtEnemies.front().second)
                {
                    NPC::hobbitProcessAnalyzer->writeData<float>(objAddrs + 0x290, hurtEnemies.front().second);
                    hurtEnemies.pop();
                    continue;
                }
            }
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

