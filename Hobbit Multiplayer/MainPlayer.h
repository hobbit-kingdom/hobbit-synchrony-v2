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
#include <limits>
#include <utility> // for std::make_pair
#include <limits>  // for std::numeric_limits
#include "Utility.h"
#include "../ServerClient/Client.h"
#include "../HobbitGameManager/HobbitGameManager.h"
#include "../HobbitGameManager/NPC.h"
#define ptrInventory 0x0075BDB0

class MainPlayer {
    HobbitProcessAnalyzer* hobbitProcessAnalyzer;
    uint32_t animation;
    Vector3 position, rotation;

    uint32_t level;
    uint32_t newLevel;

    uint32_t bilboPosXPTR;
    uint32_t bilboAnimPTR;

    float bilboAnimFrame;
    float bilboLastAnimFrame;

    int8_t bilboWeapon;

    const uint32_t X_POSITION_PTR = 0x0075BA3C;

    std::vector<std::pair<uint32_t, float>> enemies; //NPC and previous Health
    std::vector<std::pair<uint8_t, float>> inventory;

    std::atomic<bool> processPackets;

public:
    MainPlayer()
    {
    }
    void setHobbitProcessAnalyzer(HobbitGameManager& initialHobbitGameManager)
    {
        hobbitProcessAnalyzer = initialHobbitGameManager.getHobbitProcessAnalyzer();
    }
    void readPlayer(std::queue<uint8_t>& gameData)
    {
        newLevel = convertQueueToType<uint32_t>(gameData);
    }

    std::vector<BaseMessage> write() {
        std::vector<BaseMessage> messages;

        processData();

        //SNAPSHOT DATA
        messages.push_back(writeSnap());

        BaseMessage enemy = writeEnemiesEvent();
        if(enemy.message.size() >= 0)
            messages.push_back(enemy);

        return messages;
    }

    void readPtrs() {
        
        bilboPosXPTR = hobbitProcessAnalyzer->readData<uint32_t>(X_POSITION_PTR);
        bilboAnimPTR = 0x8 + hobbitProcessAnalyzer->readData<uint32_t>(0x560 + hobbitProcessAnalyzer->readData<uint32_t>(X_POSITION_PTR));
        enemies.clear();
        std::cout << "\033[34mFound Searching for Enemies" << std::endl;
        std::vector<uint32_t> allEnemieAddrs = hobbitProcessAnalyzer->findAllGameObjByPattern<uint64_t>(0x0000000200000002, 0x184 + 0x8 * 0x4); //put the values that indicate that thing
        

        for (uint32_t e : allEnemieAddrs)
        {
            if (0x04004232 != hobbitProcessAnalyzer->readData<uint32_t>(e + 0x10))
                continue;
            std::cout << std::hex << e << " || "  <<std::dec << hobbitProcessAnalyzer->readData<float>(e + 0x290) << std::endl;
            enemies.push_back(std::make_pair(e, hobbitProcessAnalyzer->readData<float>(e + 0x290)));
        }
        for (uint8_t item = 0 ; item < 58; item++)
        {
            inventory.push_back(std::make_pair(item, hobbitProcessAnalyzer->readData<float>(ptrInventory + 0x4 * item)));
        }

        std::cout << enemies.size() << " enemies" << std::endl;
        std::cout << "End of searching for Enemies" << std::endl << "\033[0m";

    }

private:
    BaseMessage writeSnap()
    {
        BaseMessage snap(SNAPSHOT_MESSAGE, 0);

        // label - Snap, 0 - reserve for size
        std::vector<uint8_t> dataVec = { static_cast<uint8_t>(DataLabel::CONNECTED_PLAYER_SNAP), 0 };
       
        pushTypeToVector(animation, dataVec);
        dataVec[1] += sizeof(animation);

        pushTypeToVector(bilboAnimFrame, dataVec);
        dataVec[1] += sizeof(bilboAnimFrame);
        pushTypeToVector(bilboLastAnimFrame, dataVec);
        dataVec[1] += sizeof(bilboLastAnimFrame);

        pushTypeToVector(position.x, dataVec);
        pushTypeToVector(position.y, dataVec);
        pushTypeToVector(position.z, dataVec);
        dataVec[1] += sizeof(position);

        pushTypeToVector(rotation.y, dataVec);
        dataVec[1] += sizeof(rotation.y);

        pushTypeToVector(bilboWeapon, dataVec);
        dataVec[1] += sizeof(bilboWeapon);
        // Convert vector to queue
        for (const uint8_t& element : dataVec) {
            snap.message.push(element);
        }
        //0x260
        std::cout << std::dec;
        std::cout <<"Sending: Weapon"<< int(bilboWeapon) << ", X " << position.x << ", Y " << position.y << ", Z " << position.z << ", R " << rotation.y << ", A " << animation << std::endl;
        return snap;
    }
    BaseMessage writeEnemiesEvent()
    {
        BaseMessage msg(EVENT_MESSAGE, 0);
        std::vector<uint8_t> dataVec = { static_cast<uint8_t>(DataLabel::ENEMIES_HEALTH), 0 };

        // amount of enemies send
        uint32_t enemisSend = 0;
        pushTypeToVector(enemisSend, dataVec);

        dataVec[1] += sizeof(uint32_t);

        for (int i = 0; i < enemies.size(); ++i)
        {

            if (enemies[i].second > hobbitProcessAnalyzer->readData<float>(enemies[i].first + 0x290))
            {
                std::cout << "Enemy: " << std::hex << enemies[i].first << std::dec;
                std::cout << "Before Health:" << enemies[i].second;
                std::cout << "After Health:" << hobbitProcessAnalyzer->readData<float>(enemies[i].first + 0x290);
                std::cout << std::endl;
                
                enemies[i].second = hobbitProcessAnalyzer->readData<float>(enemies[i].first + 0x290);
                //GUID
                pushTypeToVector(hobbitProcessAnalyzer->readData<uint64_t>(enemies[i].first + 0x8), dataVec);
                dataVec[1] += sizeof(uint64_t);

                //Heath
                pushTypeToVector(enemies[i].second, dataVec);
                dataVec[1] += sizeof(float);

                ++enemisSend;
            }
        }
        
        dataVec[2] = enemisSend;//set enemies send

        // Convert vector to queue
        for (const int& element : dataVec) {
            msg.message.push(element);
        }
        if (enemisSend > 0)
        {
            std::cout << "\033[31mSending: Enemies " << enemisSend << "\033[0m" << std::endl;
            return msg;
        }
        else
            return BaseMessage();
    }
    BaseMessage writeInventoryEvent()
    {
        BaseMessage msg(EVENT_MESSAGE, 0);
        std::vector<uint8_t> dataVec = { static_cast<uint8_t>(DataLabel::INVENTORY), 0 };

        // amount of enemies send
        uint32_t inventorySend = 0;
        pushTypeToVector(inventorySend, dataVec);

        dataVec[1] += sizeof(uint32_t);

        for (int i = 0; i < 58; i++)
        {

            if (inventory[i].second != hobbitProcessAnalyzer->readData<float>(ptrInventory + 0x4 * i))
            {
                std::cout << "Inventory: " << std::hex << ptrInventory + 0x4 * i << std::dec;
                std::cout << "Before: " << inventory[i].second;
                std::cout << "After: " << hobbitProcessAnalyzer->readData<float>(ptrInventory + 0x4 * i);
                std::cout << std::endl;

                inventory[i].second = hobbitProcessAnalyzer->readData<float>(ptrInventory + 0x4 * i);
                //Множитель указателя инвенторя
                pushTypeToVector(hobbitProcessAnalyzer->readData<uint8_t>(i), dataVec);
                dataVec[1] += sizeof(uint8_t);

                //Значение
                pushTypeToVector(inventory[i].second, dataVec);
                dataVec[1] += sizeof(float);

                ++inventorySend;
            }
        }

        dataVec[2] = inventorySend;//set enemies send

        // Convert vector to queue
        for (const int& element : dataVec) {
            msg.message.push(element);
        }
        if (inventorySend > 0)
        {
            std::cout << "\033[31mSending: Item " << inventorySend << "\033[0m" << std::endl;
            return msg;
        }
        else
            return BaseMessage();
    }
    void processData()
    {
        position.x = hobbitProcessAnalyzer->readData<float>(0x7C4 + bilboPosXPTR);
        position.y = hobbitProcessAnalyzer->readData<float>(0x7C8 + bilboPosXPTR);
        position.z = hobbitProcessAnalyzer->readData<float>(0x7CC + bilboPosXPTR);
        rotation.y = hobbitProcessAnalyzer->readData<float>(0x7AC + bilboPosXPTR);
        animation = hobbitProcessAnalyzer->readData<uint32_t>(bilboAnimPTR);

        bilboAnimFrame = hobbitProcessAnalyzer->readData<float>(0x0075BA3C + 0x530);
        bilboLastAnimFrame = hobbitProcessAnalyzer->readData<float>(0x0075BA3C + 0x53C);

        bilboWeapon = hobbitProcessAnalyzer->readData<int8_t>(0x0075C738);
    }
};
