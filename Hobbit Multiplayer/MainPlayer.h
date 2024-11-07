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
    const uint32_t X_POSITION_PTR = 0x0075BA3C;

    std::vector<std::pair<NPC, float>> enemies; //NPC and previous Health


    std::atomic<bool> processPackets;

public:
    void readPlayer(std::queue<uint8_t>& gameData)
    {
        newLevel = convertQueueToType<uint32_t>(gameData);
    }

    std::vector<BaseMessage> write() {
        std::vector<BaseMessage> messages;

        processData();
        //SNAPSHOT DATA
        messages.push_back(writeSnap());
        messages.push_back(writeEnemiesEvent());

        return messages;
    }

    void setHobbitProcessAnalyzer(HobbitProcessAnalyzer &newHobbitProcessAnalyzer)
    {
        hobbitProcessAnalyzer = &newHobbitProcessAnalyzer;
    }
    void readPtrs() {
        bilboPosXPTR = hobbitProcessAnalyzer->readData<uint32_t>(X_POSITION_PTR);
        bilboAnimPTR = 0x8 + hobbitProcessAnalyzer->readData<uint32_t>(0x560 + hobbitProcessAnalyzer->readData<uint32_t>(X_POSITION_PTR));

        std::cout << "Searching for Enemies" << std::endl;
        std::vector<uint32_t> allEnemieAddrs = hobbitProcessAnalyzer->findAllGameObjByPattern<uint8_t>(0x02, 0x184 + 0x8 * 0x4); //put the values that indicate that thing
        std::cout << "Found " << allEnemieAddrs.size() << " enemis" << std::endl;
        for (uint32_t e : allEnemieAddrs)
        {
            NPC npc;
            uint64_t guid = hobbitProcessAnalyzer->readData<uint64_t>(e + 0x8);
            npc.setNCP(guid);
            enemies.push_back(std::make_pair(npc, npc.getHealth()));
        }
        std::cout << "End of searching for Enemies" << std::endl;

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

        // Convert vector to queue
        for (const int& element : dataVec) {
            snap.message.push(element);
        }

        std::cout << std::dec;
        std::cout << "Sending: X " << position.x << ", Y " << position.y << ", Z " << position.z << ", R " << rotation.y << ", A " << animation << std::endl;
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

        for (std::pair<NPC, float> e : enemies)
        {
            if (e.second > e.first.getHealth())
            {
                //GUID
                pushTypeToVector(e.first.getGUID(), dataVec);
                dataVec[1] += sizeof(e.first.getGUID());

                //Heath
                pushTypeToVector(e.first.getHealth(), dataVec);
                dataVec[1] += sizeof(e.first.getHealth());

                ++enemisSend;
            }
        }

        dataVec[2] = enemisSend;//set enemies send

        // Convert vector to queue
        for (const int& element : dataVec) {
            msg.message.push(element);
        }

        std::cout << "Sending: Enemies " << enemisSend << std::endl;
        return msg;
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

        for (std::pair<NPC, uint32_t> e : enemies)
            e.second = e.first.getHealth();
    }
};
