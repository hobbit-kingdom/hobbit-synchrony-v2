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

    float bilboAnimFrame;
    float bilboLastAnimFrame;
    const uint32_t X_POSITION_PTR = 0x0075BA3C;

    std::vector<std::pair<NPC, NPC>> enemies; //before and after


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
        

        return messages;
    }

    void setHobbitProcessAnalyzer(HobbitProcessAnalyzer &newHobbitProcessAnalyzer)
    {
        hobbitProcessAnalyzer = &newHobbitProcessAnalyzer;
    }
    void readPtrs() {
        bilboPosXPTR = hobbitProcessAnalyzer->readData<uint32_t>(X_POSITION_PTR, 4);
        bilboAnimPTR = 0x8 + hobbitProcessAnalyzer->readData<uint32_t>(0x560 + hobbitProcessAnalyzer->readData<uint32_t>(X_POSITION_PTR, 4), 4);

        //find all enemies in the level
        //std::vector<std::pair<NPC, NPC>> enemies;
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

        for (std::pair<NPC, NPC> npc : enemies)
        {
            if (npc.first.getHealth() >= npc.second.getHealth())
            {
                pushTypeToVector(npc.second.getGUID(), dataVec);
                dataVec[1] += sizeof(npc.second.getGUID());

                pushTypeToVector(npc.second.getHealth(), dataVec);
                dataVec[1] += sizeof(npc.second.getHealth());
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
        position.x = hobbitProcessAnalyzer->readData<float>(0x7C4 + bilboPosXPTR, 4);
        position.y = hobbitProcessAnalyzer->readData<float>(0x7C8 + bilboPosXPTR, 4);
        position.z = hobbitProcessAnalyzer->readData<float>(0x7CC + bilboPosXPTR, 4);
        rotation.y = hobbitProcessAnalyzer->readData<float>(0x7AC + bilboPosXPTR, 4);
        animation = hobbitProcessAnalyzer->readData<uint32_t>(bilboAnimPTR, 4);

        bilboAnimFrame = hobbitProcessAnalyzer->readData<float>(0x0075BA3C + 0x530, 4);
        bilboLastAnimFrame = hobbitProcessAnalyzer->readData<float>(0x0075BA3C + 0x53C, 4);

    }
};
