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

#include "MessageProcessing.h"
#include "../ServerClient/Client.h"
#include "../HobbitGameManager/HobbitGameManager.h"
#include "../HobbitGameManager/NPC.h"

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

// Player classes
class ConnectedPlayer {
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
};
class MainPlayer {
    HobbitProcessAnalyzer *hobbitProcessAnalyzer;
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
    std::vector<uint8_t> write() {
        // label - Snap, 0 - reserve for size
        std::vector<uint8_t> data = { static_cast<uint8_t>(DataLabel::CONNECTED_PLAYER_SNAP), 0 };

        // Prepares packets to send
        position.x = hobbitProcessAnalyzer->readData<float>(0x7C4 + bilboPosXPTR, 4);
        position.y = hobbitProcessAnalyzer->readData<float>(0x7C8 + bilboPosXPTR, 4);
        position.z = hobbitProcessAnalyzer->readData<float>(0x7CC + bilboPosXPTR, 4);
        rotation.y = hobbitProcessAnalyzer->readData<float>(0x7AC + bilboPosXPTR, 4);
        uint32_t animBilbo = hobbitProcessAnalyzer->readData<uint32_t>(bilboAnimPTR, 4);

        pushTypeToVector(animation, data);
        data[1] += sizeof(animation);

        pushTypeToVector(position.x, data);
        pushTypeToVector(position.y, data);
        pushTypeToVector(position.z, data);
        data[1] += sizeof(position);

        pushTypeToVector(rotation.y, data);
        data[1] += sizeof(rotation.y);

        return data;
    }

    void setHobbitProcessAnalyzer(HobbitProcessAnalyzer* newHobbitProcessAnalyzer)
    {
        hobbitProcessAnalyzer = newHobbitProcessAnalyzer;
    }
    void readPtrs() {
        bilboPosXPTR = hobbitProcessAnalyzer->readData<uint32_t>(X_POSITION_PTR, 4);
        bilboAnimPTR = 0x8 + hobbitProcessAnalyzer->readData<uint32_t>(0x560 + hobbitProcessAnalyzer->readData<uint32_t>(X_POSITION_PTR, 4), 4);
    }
};

// Main client class
class HobbitClient {
public:
    HobbitClient(std::string initialServerIp = "")
        : serverIp(std::move(initialServerIp)), running(false), processMessages(false) {
    }

    ~HobbitClient() { stop(); }

    int start();
    int start(const std::string& ip);
    void stop();

private:
    Client client;
    HobbitGameManager hobbitGameManager;
    std::vector<uint64_t> guids;
    std::string serverIp;
    std::atomic<bool> running;
    std::atomic<bool> processMessages;
    std::thread updateThread;

    static constexpr int MAX_PLAYERS = 7;
    ConnectedPlayer connectedPlayer[MAX_PLAYERS];
    MainPlayer mainPlayer;
    
    void update();
    void readMessage();
    void writeMessage();
    void processGameData(int senderID, std::queue<uint8_t>& gameData);
    MessageBundle createMessages();

    void onEnterNewLevel() { 
        processMessages = true;    

        mainPlayer.readPtrs(); 

        for (int i = 0; i < MAX_PLAYERS; ++i)
        {
            connectedPlayer[i].npc.setNCP(guids[i], hobbitGameManager.getHobbitProcessAnalyzer());
        }
    }
    void onExitLevel() { processMessages = false; }
    void onOpenGame() 
    { 
        processMessages = false; 

        mainPlayer.setHobbitProcessAnalyzer(hobbitGameManager.getHobbitProcessAnalyzer());
        mainPlayer.readPtrs();

        guids = getPlayersNpcGuid();
        for (int i = 0; i < MAX_PLAYERS; ++i)
        {
            connectedPlayer[i].npc.setNCP(guids[i], hobbitGameManager.getHobbitProcessAnalyzer());
        }
    }
    void onCloseGame() { /* Add closing game logic here if needed */ }

    void onClientListUpdate(const std::vector<uint8_t>&) {
        for (int i = 0; i < MAX_PLAYERS; ++i) {
            connectedPlayer[i].id = (i < client.connectedClients.size()) ? client.connectedClients[i] : -1;
        }
    }

    std::vector<uint64_t> getPlayersNpcGuid() {
        std::ifstream file;
        std::string filePath = "FAKE_BILBO_GUID.txt";

        // Open the file, prompting the user if it doesn't exist
        while (!file.is_open()) {
            file.open(filePath);
            if (!file.is_open()) {
                std::cout << "File not found. Enter the path to FAKE_BILBO_GUID.txt or 'q' to quit: ";
                std::string input;
                std::getline(std::cin, input);
                if (input == "q") return {}; // Quit if user enters 'q'
                filePath = input;
            }
        }

        std::vector<uint64_t> tempGUID;
        std::string line;

        // Read each line from the file
        while (std::getline(file, line)) {
            // Find the position of the underscore
            size_t underscorePos = line.find('_');
            if (underscorePos != std::string::npos) {
                // Split the line into two parts
                std::string part2 = line.substr(0, underscorePos); // Before the underscore
                std::string part1 = line.substr(underscorePos + 1); // After the underscore

                // Concatenate the parts in reverse order
                std::string combined = part2 + part1;

                // Convert the combined string to uint64_t
                uint64_t guid = std::stoull(combined, nullptr, 16); // Convert from hex string to uint64_t
                tempGUID.push_back(guid);
            }
        }

        std::cout << "FOUND FILE!" << std::endl;
        return tempGUID;
    }

};

int HobbitClient::start() {
    std::cout << "Enter server IP address: ";
    std::cin >> serverIp;
    std::cin.ignore();

    return start(serverIp);
}
int HobbitClient::start(const std::string& ip) {
    serverIp = ip;

    client.addListener([this](const std::vector<uint8_t>& clientIDs) {
        onClientListUpdate(clientIDs);
        });

    if (client.start(serverIp)) return 1;

    while (!hobbitGameManager.isGameRunning()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "Please open The Hobbit 2003 game!" << std::endl;
    }

   

    running = true;
    hobbitGameManager.addListenerEnterNewLevel([this] { onEnterNewLevel(); });
    hobbitGameManager.addListenerExitLevel([this] { onExitLevel(); });
    hobbitGameManager.addListenerOpenGame([this] { onOpenGame(); });
    hobbitGameManager.addListenerCloseGame([this] { onCloseGame(); });

    onOpenGame();
    updateThread = std::thread(&HobbitClient::update, this);
    return 0;
}

void HobbitClient::stop() {
    running = false;
    client.stop();

    if (updateThread.joinable()) {
        updateThread.join();
    }
}
void HobbitClient::update() {
    while (running) {
        readMessage();
        for (int i = 0; i < MAX_PLAYERS; ++i)
        {
            connectedPlayer[i].processPlayer(client.getClientID());
        }
        writeMessage();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

void HobbitClient::readMessage() {
    if (auto textMessageOpt = client.getTextMessage()) {
        std::cout << "Received Text Message from " << int(textMessageOpt->senderID) << ": "
            << std::string(textMessageOpt->text.begin(), textMessageOpt->text.end()) << std::endl;
    }

    if (auto eventMessageOpt = client.getEventMessage()) {
        processGameData(eventMessageOpt->senderID, eventMessageOpt->eventData);
    }

    for (auto& [clientID, snapshotMsg] : client.getSnapshotMessages()) {
        processGameData(clientID, snapshotMsg.snapshotData);
    }
}
void HobbitClient::writeMessage() {
    MessageBundle messages = createMessages();

    if (messages.textResponse) client.sendMessage(messages.textResponse);
    if (messages.eventResponse) client.sendMessage(messages.eventResponse);
    if (messages.snapshotResponse) client.sendMessage(messages.snapshotResponse);

    delete messages.textResponse;
    delete messages.eventResponse;
    delete messages.snapshotResponse;
}

void HobbitClient::processGameData(int senderID, std::queue<uint8_t>& gameData) {
    std::cout << "Received Message from client " << senderID << std::endl;
    
    while (!gameData.empty()) {
        DataLabel label = static_cast<DataLabel>(gameData.front());
        gameData.pop();

        int size_tmp = gameData.front();
        gameData.pop();
        if (label == DataLabel::SERVER)
        {

        }
        else if(label == DataLabel::CONNECTED_PLAYER_SNAP)
        {
       
            auto it = std::find_if(std::begin(connectedPlayer), std::end(connectedPlayer),
                [&](const ConnectedPlayer& p) { return p.id == senderID; });
            if (it != std::end(connectedPlayer)) {
                it->readConectedPlayerSnap(gameData);
            }
            else {
                std::cerr << "ERROR: Unregistered player id: " << senderID << std::endl;
                connectedPlayer[0].readConectedPlayerSnap(gameData);
            }

        }
        else if (label == DataLabel::CONNECTED_PLAYER_LEVEL)
        {
            auto it = std::find_if(std::begin(connectedPlayer), std::end(connectedPlayer),
                [&](const ConnectedPlayer& p) { return p.id == senderID; });
            if (it != std::end(connectedPlayer)) {
                it->readConectedPlayerSnap(gameData);
            }
            else {
                std::cerr << "ERROR: Unregistered player id: " << senderID << std::endl;
                connectedPlayer[0].readConectedPlayerSnap(gameData);
            }
        }
        else
        {
            std::cerr << "ERROR: Unknown label received" << std::endl;
        }
    }
}

MessageBundle HobbitClient::createMessages() {
    static int counter = 0;
    ++counter;

    std::vector<uint8_t> dataSnapshot = mainPlayer.write();
    //label = Level, size = 3, data 3 times
    //std::vector<uint8_t> dataSnapshot = { static_cast<uint8_t>(DataLabel::CONNECTED_PLAYER), 3,
    //                                      static_cast<uint8_t>(counter),
    //                                      static_cast<uint8_t>(counter * 2),
    //                                      static_cast<uint8_t>(counter + 30) };

    MessageBundle messages;
    messages.textResponse = nullptr;
    messages.eventResponse = nullptr;
    messages.snapshotResponse = new EventMessage(0, dataSnapshot);
    return messages;
}
