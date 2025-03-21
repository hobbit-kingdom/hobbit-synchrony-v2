#include "HobbitClient.h"

HobbitClient::HobbitClient(std::string initialServerIp)
	: serverIp(std::move(initialServerIp)), running(false), processMessages(false),
	logOption_(LogManager::Instance().CreateLogOption("HOBBIT CLIENT")) {

	LogManager::Instance().MoveLogOption("CLIENT", "HOBBIT CLIENT");
	LogManager::Instance().MoveLogOption("HOBBIT GAME MANAGER", "HOBBIT CLIENT");

	for (auto& e : connectedPlayers) {
		e.setHobbitProcessAnalyzer(hobbitGameManager);
	}
	mainPlayer.setHobbitProcessAnalyzer(hobbitGameManager);
}


HobbitClient::~HobbitClient() { stop(); }
int HobbitClient::start() {
	std::ifstream file("./server_ip.txt");
	if (file) {
		// Read the first line from server_ip.txt if it exists
		std::getline(file, serverIp);

		if (!serverIp.empty()) {
			logOption_->LogMessage(LogLevel::Log_Prompt, "Using server IP from file: " + serverIp);
		}
		else {
			// File is empty, proceed with user input
			logOption_->LogMessage(LogLevel::Log_Prompt, "server_ip.txt is empty. Enter server IP address: ");
			std::cin >> serverIp;
			std::cin.ignore();
		}
	}
	else {
		// File doesn't exist, prompt for IP address
		logOption_->LogMessage(LogLevel::Log_Prompt, "Enter server IP address: ");
		std::cin >> serverIp;
		std::cin.ignore();
	}

	return start(serverIp);
}

int HobbitClient::start(const std::string& ip) {
	serverIp = ip;

	client.addListener([this](const std::queue<uint8_t>& clientIDs) {
		onClientListUpdate(clientIDs);
		});

	if (client.start(serverIp)) return 1;

	guids = getPlayersNpcGuid();

	while (!hobbitGameManager.isGameRunning()) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
		logOption_->LogMessage(LogLevel::Log_Prompt, "You must open The Hobbit 2003 game!");
	}

	running = true;
	hobbitGameManager.addListenerEnterNewLevel([this] { onEnterNewLevel(); });
	hobbitGameManager.addListenerExitLevel([this] { onExitLevel(); });
	hobbitGameManager.addListenerOpenGame([this] { onOpenGame(); });
	hobbitGameManager.addListenerCloseGame([this] { onCloseGame(); });

	hobbitGameManager.start();

	onOpenGame();

	updateThread = std::thread(&HobbitClient::update, this);
	return 0;
}

void HobbitClient::stop() {
	running = false;
	client.stop();
	hobbitGameManager.stop();

	if (updateThread.joinable()) {
		updateThread.join();
	}
}void HobbitClient::update() {
	while (running) {
		if (processMessages)
		{
			if (!hobbitGameManager.isOnLevel())
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
				continue;
			}
		}
		else
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			continue;
		}
		readMessage();
		for (int i = 0; i < MAX_PLAYERS; ++i)
		{
			connectedPlayers[i].processPlayer(client.getClientID());
		}
		writeMessage();
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}
}

void HobbitClient::readMessage() {

	BaseMessage textMessageOpt = client.frontTextMessage();
	if (textMessageOpt.message.size() > 0) {
		// Assuming textMessageOpt.message is a queue of characters or strings
		std::string fullMessage;
		while (!textMessageOpt.message.empty()) {
			fullMessage += textMessageOpt.message.front(); // Get the front message
			textMessageOpt.message.pop(); // Remove the front message from the queue
		}

		logOption_->LogMessage(LogLevel::Log_Debug, "Received Text Message from", int(textMessageOpt.senderID), ":", fullMessage);

		client.popFrontTextMessage();
	}

	BaseMessage eventMessageOpt = client.frontEventMessage();
	if (eventMessageOpt.message.size() > 0) {
		readGameMessage(eventMessageOpt.senderID, eventMessageOpt.message);
		client.popFrontEventMessage();
	}

	std::map<uint8_t, BaseMessage> snapshotMessages = client.snapMessage();
	for (auto& pair : snapshotMessages) {
		readGameMessage(pair.first, pair.second.message);
		client.clearSnapMessage();
	}

}
void HobbitClient::writeMessage() {

	std::vector<BaseMessage> messages;

	std::vector<BaseMessage> mainPlayerMsg = mainPlayer.write();
	std::vector<BaseMessage> connectedPlayerMsg = mainPlayer.write();

	messages.insert(messages.end(), mainPlayerMsg.begin(), mainPlayerMsg.end());
	messages.insert(messages.end(), connectedPlayerMsg.begin(), connectedPlayerMsg.end());

	for (BaseMessage& e : messages)
	{
		e.senderID = client.getClientID();

		if (e.message.size() > 0)
			client.sendMessage(e);
	}
}
void HobbitClient::readGameMessage(int senderID, std::queue<uint8_t>& gameData) {
	logOption_->LogMessage(LogLevel::Log_Debug, "Received Game Massage from", senderID);

	while (!gameData.empty()) {
		DataLabel label = static_cast<DataLabel>(gameData.front());
		gameData.pop();

		uint8_t size_tmp = gameData.front();
		gameData.pop();

		if (label == DataLabel::CONNECTED_PLAYER_SNAP)
		{
			auto it = std::find_if(std::begin(connectedPlayers), std::end(connectedPlayers),
				[&](const ConnectedPlayer& p) { return p.id == senderID; });
			if (it != std::end(connectedPlayers)) {
				it->readConectedPlayerSnap(gameData);
			}
			else {
				logOption_->LogMessage(LogLevel::Log_Error, "Unregistered player id", senderID);
				connectedPlayers[0].readConectedPlayerSnap(gameData);
			}

		}
		else if (label == DataLabel::ENEMIES_HEALTH)
		{
			auto it = std::find_if(std::begin(connectedPlayers), std::end(connectedPlayers),
				[&](const ConnectedPlayer& p) { return p.id == senderID; });
			if (it != std::end(connectedPlayers)) {
				it->readProcessEnemiesHealth(gameData);
			}
			else {
				logOption_->LogMessage(LogLevel::Log_Error, "Unregistered player id", senderID);
				connectedPlayers[0].readProcessEnemiesHealth(gameData);
			}
		}
		else if (label == DataLabel::INVENTORY)
		{
			mainPlayer.readProcessInventory(gameData);
		}
		else
		{
			logOption_->LogMessage(LogLevel::Log_Error, "Unknown label received", int(label));
		}
	}
}


void HobbitClient::onEnterNewLevel() {

	std::this_thread::sleep_for(std::chrono::seconds(5));

	hobbitGameManager.getHobbitProcessAnalyzer()->updateObjectStackAddress();


	if (guids.size() == 0)
		guids = getPlayersNpcGuid();

	for (int i = 0; i < MAX_PLAYERS; ++i)
	{
		connectedPlayers[i].npc.setNCP(guids[i]);
	}

	mainPlayer.readPtrs();

	processMessages = true;
}
void HobbitClient::onOpenGame()
{
	processMessages = false;

	if (hobbitGameManager.isOnLevel())
		onEnterNewLevel();
}


// Modified onClientListUpdate to use listener parameter
void HobbitClient::onClientListUpdate(const std::queue<uint8_t>& clientIDs) {
	running = !clientIDs.empty();

	// Create a copy of the client IDs queue
	std::queue<uint8_t> connectedClients = clientIDs;

	for (int i = 0; i < MAX_PLAYERS; ++i) {
		connectedPlayers[i].id = connectedClients.empty() ? -1 : connectedClients.front();
		if (!connectedClients.empty()) {
			connectedClients.pop();
		}
	}
}
std::vector<uint64_t> HobbitClient::getPlayersNpcGuid() {
	std::ifstream file;
	std::string filePath = "FAKE_BILBO_GUID.txt";
	bool foudFileInitially = true;;
	// Open the file, prompting the user if it doesn't exist
	while (!file.is_open()) {
		file.open(filePath);
		if (!file.is_open()) {
			foudFileInitially = false;
			logOption_->LogMessage(LogLevel::Log_Prompt, "File not found. Enter the path to FAKE_BILBO_GUID.txt or 'q' to quit: ");
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
	if (foudFileInitially)
		logOption_->LogMessage(LogLevel::Log_Info, "FOUND FILE!");
	return tempGUID;
}