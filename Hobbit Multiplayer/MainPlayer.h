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
#include "../LogSystem/LogManager.h"

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

    LogOption::Ptr logOption_;
public:
    MainPlayer() : logOption_(LogManager::Instance().CreateLogOption("MAIN PLAYER"))
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
        if(enemy.message.size() > 0)
            messages.push_back(enemy);

        BaseMessage inventory = writeInventoryEvent();
        if (inventory.message.size() > 0)
            messages.push_back(inventory);

        return messages;
    }

    void readPtrs() {
        //Biblbo Pointers
        bilboPosXPTR = hobbitProcessAnalyzer->readData<uint32_t>(X_POSITION_PTR);
        bilboAnimPTR = 0x8 + hobbitProcessAnalyzer->readData<uint32_t>(0x560 + hobbitProcessAnalyzer->readData<uint32_t>(X_POSITION_PTR));
        
        //Enemies 
        enemies.clear();
        logOption_->LogMessage(LogLevel::Log_Debug, "List Enemies");
        logOption_->increaseDepth();
        std::vector<uint32_t> allEnemieAddrs = hobbitProcessAnalyzer->findAllGameObjByPattern<uint64_t>(0x0000000200000002, 0x184 + 0x8 * 0x4); //put the values that indicate that thing
        
        for (uint32_t e : allEnemieAddrs)
        {
            if (0x04004232 != hobbitProcessAnalyzer->readData<uint32_t>(e + 0x10))
                continue;
            //hex
            logOption_->LogMessage(LogLevel::Log_Debug, "Address:", e, "Health: ", hobbitProcessAnalyzer->readData<float>(e + 0x290));
            enemies.push_back(std::make_pair(e, hobbitProcessAnalyzer->readData<float>(e + 0x290)));
        }
        logOption_->decreaseDepth();
        logOption_->LogMessage(LogLevel::Log_Debug, "Enemis Foud:", enemies.size());

        //Items
        logOption_->LogMessage(LogLevel::Log_Debug, "List Items");
        logOption_->increaseDepth();
        for (uint8_t item = 0 ; item < 56; item++)
        {
            logOption_->LogMessage(LogLevel::Log_Debug, "Address:", ptrInventory + 0x4 * item, "Value: ", hobbitProcessAnalyzer->readData<float>(ptrInventory + 0x4 * item));
            inventory.push_back(std::make_pair(item, hobbitProcessAnalyzer->readData<float>(ptrInventory + 0x4 * item)));
        }
        logOption_->decreaseDepth();
    }
    void readProcessInventory(std::queue<uint8_t>& gameData)
    {
        uint32_t numberChangeInventory = convertQueueToType<uint32_t>(gameData);
        std::queue<std::pair<uint8_t, float>> readInventory;
        for (int i = 0; i < numberChangeInventory; i++)
        {
            uint8_t item = convertQueueToType<uint8_t>(gameData);
            float value = convertQueueToType<float>(gameData);
            readInventory.push(std::pair(item, value));
        }

        while (!readInventory.empty())
        {
            logOption_->LogMessage(LogLevel::Log_Debug, "Inventory Changed");
            logOption_->increaseDepth();
            logOption_->LogMessage(LogLevel::Log_Debug, "GUID:", readInventory.front().first * 0x4 + ptrInventory, "New Value:", readInventory.front().second);
            logOption_->decreaseDepth();

            //check if the object exist
            float value = hobbitProcessAnalyzer->readData<float>(ptrInventory + 0x4 * readInventory.front().first);
            if (value != readInventory.front().second)
            {
                hobbitProcessAnalyzer->writeData<float>(ptrInventory + 0x4 * readInventory.front().first, readInventory.front().second);
                inventory.at(readInventory.front().first).second = readInventory.front().second;
                readInventory.pop();
                continue;
            }
            readInventory.pop();
        }
        logOption_->decreaseDepth();
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

        logOption_->LogMessage(LogLevel::Log_Debug, "Sending Msg", "size", int(dataVec[1]), "Anim", animation, "Anim Frames", bilboAnimFrame, bilboLastAnimFrame, "Pos", position.x, position.y, position.z, "RotY", rotation.y, "Weapon", bilboWeapon);
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
                logOption_->LogMessage(LogLevel::Log_Debug, "Write Enemy Address", enemies[i].first);
                logOption_->increaseDepth();
                logOption_->LogMessage(LogLevel::Log_Debug, "Heath: Before", enemies[i].second, "After", hobbitProcessAnalyzer->readData<float>(enemies[i].first + 0x290));
                logOption_->decreaseDepth();
                
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
            logOption_->LogMessage(LogLevel::Log_Debug, "Sending: Enemies sent", enemisSend);
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

        for (uint8_t i = 0; i < 56; i++)
        {
            if (i > 1 and i < 6) i = 6;
            else if (i > 7 and i < 20) i = 20;
            else if (i > 22 and i < 25) i = 25;
            else if (i > 25 and i < 28) i = 28;
            else if (i == 46) i++;
            else if (i == 50) i = 52;
            if ((inventory[i].second != hobbitProcessAnalyzer->readData<float>(ptrInventory + 0x4 * i) and i <53) or (i>=53 and inventory[i].second > hobbitProcessAnalyzer->readData<float>(ptrInventory + 0x4 * i)))
            {
                if (i == 6 and inventory[i].second > hobbitProcessAnalyzer->readData<float>(ptrInventory + 0x4 * i))
                    continue;
                if (i == 7 and inventory[25].second > hobbitProcessAnalyzer->readData<float>(ptrInventory + 0x4 * 25))
                    continue;
                else if (i == 7 and inventory[i].second > hobbitProcessAnalyzer->readData<float>(ptrInventory + 0x4 * i))
                    continue;
                //hex
                logOption_->LogMessage(LogLevel::Log_Debug, "Write Inventory Address", ptrInventory + 0x4 * i);
                logOption_->increaseDepth();
                logOption_->LogMessage(LogLevel::Log_Debug, "Value: Before", inventory[i].second, "After", hobbitProcessAnalyzer->readData<float>(ptrInventory + 0x4 * i));
                logOption_->decreaseDepth();
                inventory[i].second = hobbitProcessAnalyzer->readData<float>(ptrInventory + 0x4 * i);
                //Множитель указателя инвенторя
                pushTypeToVector(i, dataVec);
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
            logOption_->LogMessage(LogLevel::Log_Debug, "Sending: Items sent", int(inventorySend));
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
