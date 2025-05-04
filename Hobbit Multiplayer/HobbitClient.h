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
#include <unordered_set>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem> // Include the filesystem library

#include "../ServerClient/Client.h"
#include "../HobbitGameManager/HobbitGameManager.h"
#include "../HobbitGameManager/NPC.h"
#include "../LogSystem/LogManager.h"

#include "Utility.h"
#include "MainPlayer.h"
#include "ConnectedPlayer.h"

// Main client class
class HobbitClient {
public:
	HobbitClient(std::string initialServerIp = "");
	~HobbitClient();

	int start();
	int start(const std::string& ip);
	void stop();

	bool isRunning() { return running; }
	// Add these methods
	std::map<DataLabel, bool> getMessageLabelStates() const;
	void setMessageLabelProcessing(DataLabel label, bool enable);
private:
	std::map<DataLabel, bool> messageLabelStates;

	LogOption::Ptr logOption_;
	Client client;
	HobbitGameManager hobbitGameManager;

	std::vector<uint64_t> guids;

	std::string serverIp;


	std::thread updateThread;
	std::atomic<bool> running;
	std::atomic<bool> processMessages = false;


	static constexpr int MAX_PLAYERS = 7;
	ConnectedPlayer connectedPlayers[MAX_PLAYERS];
	MainPlayer mainPlayer;

	void update();
	void readMessage();
	void readGameMessage(int senderID, std::queue<uint8_t>& gameData);
	void writeMessage();

	void onEnterNewLevel();
	void onExitLevel() { processMessages = false; }

	void onOpenGame();
	void onCloseGame() { processMessages = false; stop(); }

	void onClientListUpdate(const std::queue<uint8_t>&);

	std::vector<uint64_t> getPlayersNpcGuid();
};
