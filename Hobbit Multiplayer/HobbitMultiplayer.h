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
	int mainMenu();
	int startMenu();

	void startServer();
	void startClient();
	void sstartServerClient();

	LogOption::Ptr logOption_;
	Server server;
	HobbitClient hobbitClient;
};

