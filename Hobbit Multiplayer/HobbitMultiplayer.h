#pragma once
#include <iostream>
#include <thread>
#include <atomic>
#include <optional> // Make sure to include this for std::optional

#include "HobbitClient.h"
#include "../ServerClient/Server.h"
#include "../ServerClient/IPv4.h"

//THIS #include <windows.h> MUST BE AFTER THE ../ServerClient AND ../HobbitGameManager
#include <windows.h>

class HobbitMultiplayer
{
public:
	HobbitMultiplayer() :logOption_(LogManager::Instance().CreateLogOption("HOBBIT MULTIPLAYER"))
	{
		LogManager::Instance().MoveLogOption("SERVER", "HOBBIT MULTIPLAYER");
		LogManager::Instance().MoveLogOption("HOBBIT CLIENT", "HOBBIT MULTIPLAYER");
		LogManager::Instance().DisplayHierarchy();
	}
	int mainMenu();
	int startMenu();

	void startServer();
	void stopServer();
	int startClient(const std::string& ip = "");
	void stopClient();
	void startServerClient();


	std::map<DataLabel, bool> getMessageLabelStates() const;
	void setMessageLabelProcessing(DataLabel label, bool enable);

	LogOption::Ptr logOption_;
	Server server;
	HobbitClient hobbitClient;
};

