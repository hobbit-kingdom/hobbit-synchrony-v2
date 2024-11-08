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

#define ptrInventory 0x0075BDB0

// Player classes
class ConnectedPlayer {
    HobbitProcessAnalyzer* hobbitProcessAnalyzer;
    uint32_t animation;
    float animFrame, lastAnimFrame;
    Vector3 position, rotation;

    uint32_t level;
    uint32_t levels;
    int8_t weapon;

    std::queue<std::pair<uint64_t, float>> hurtEnemies;// pair <guid, Health>
    std::queue<std::pair<uint8_t, float>> inventory;
public:
    ConnectedPlayer()
    {

    }
    void setHobbitProcessAnalyzer(HobbitGameManager& initialHobbitGameManager)
    {
        hobbitProcessAnalyzer = initialHobbitGameManager.getHobbitProcessAnalyzer();
        npc.setHobbitProcessAnalyzer(hobbitProcessAnalyzer);
    }
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
    void readProcessInventory(std::queue<uint8_t>& gameData)
    {
        uint32_t numberChangeInventory = convertQueueToType<uint32_t>(gameData);

        for (int i = 0; i < numberChangeInventory; i++)
        {
            uint8_t item = convertQueueToType<uint8_t>(gameData);
            float value = convertQueueToType<float>(gameData);
            inventory.push(std::pair(item, value));
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

        // Display the data
        std::cout << "\033[33m";
        std::cout << "Recieve the packet Send: " << std::endl;
        std::cout << "Weapon" << int(weapon) << " || ";
        std::cout << "X: " << position.x << " || ";
        std::cout << "Y: " << position.y << " || ";
        std::cout << "Z: " << position.z << " || ";
        std::cout << "R: " << rotation.y << " || ";
        std::cout << "A: " << animation << std::endl << std::endl;
        std::cout << "\033[0m";

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


        uint64_t guidNone = 0x0;
        if (weapon != -1)
        {
            //NONE
            if (weapon == 2)
                guidNone = 0x0D8AD910E885100D;
            //STING
            else if (weapon == 0)
                guidNone = 0x0D8AD910E885100B;
            //STAFF
            else if (weapon == 1)
                guidNone = 0x0D8AD910E885100A;
            //STONE
            else if (weapon == 3)
                guidNone = 0x0D8AD910E885100C;

            uint32_t addrsGuidNone = hobbitProcessAnalyzer->findGameObjByGUID(guidNone);
            if (addrsGuidNone)
                npc.setWeapon(hobbitProcessAnalyzer->readData<uint32_t>(addrsGuidNone + 0x260));
        }
       


        // Update Health of Enemies
        while (!hurtEnemies.empty())
        {
            if (hurtEnemies.front().first == 0)
            {
                hurtEnemies.pop();
                continue;
            }
            std::cout << "\033[31mENEMY HURT!!  ";
            std::cout << std::hex << hurtEnemies.front().first << std::dec << " " << hurtEnemies.front().second;
            std::cout << "\033[0m";
            //find by guid
            uint32_t objAddrs = hobbitProcessAnalyzer->findGameObjByGUID(hurtEnemies.front().first);

            //check if the object exist
            if (objAddrs != 0)
            {
                float health = hobbitProcessAnalyzer->readData<float>(objAddrs + 0x290);
                if (health > hurtEnemies.front().second)
                {
                    hobbitProcessAnalyzer->writeData<float>(objAddrs + 0x290, hurtEnemies.front().second);
                    hurtEnemies.pop();
                    continue;
                }
            }
            hurtEnemies.pop();

        }

        while (!inventory.empty())
        {
            std::cout << "\033[31mITEM CHANGE!!  ";
            std::cout << std::hex << inventory.front().first * 0x4 + ptrInventory << std::dec << " " << inventory.front().second;
            std::cout << "\033[0m";
          
            //check if the object exist
            float value = hobbitProcessAnalyzer->readData<float>(ptrInventory + 0x4 * inventory.front().first);
            if (value != inventory.front().second)
            {
                hobbitProcessAnalyzer->writeData<float>(ptrInventory + 0x4 * inventory.front().first, inventory.front().second);
                inventory.pop();
                continue;
            }
            inventory.pop();
        }
    }


    std::vector<BaseMessage> write() {
        //In case you need to send some state from the npc bilbo (which is unlickely) 
        return std::vector<BaseMessage>();
    }

    void clear() { id = -1; }
};