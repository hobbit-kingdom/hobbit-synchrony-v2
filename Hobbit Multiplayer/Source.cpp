#define DEBUG
#include <iostream>
#include <thread>
#include <atomic>
#include <optional> // Make sure to include this for std::optional

#include "HobbitClient.h"
#include "../ServerClient/Server.h"
#include "../ServerClient/IPv4.h"
#include "../LogSystem/LogManager.h"

//THIS #include <windows.h> MUST BE AFTER THE ../ServerClient AND ../HobbitGameManager
#include <windows.h>

using namespace std;

LogOption::Ptr logOption_;
int mainMenu()
{
	logOption_ = LogManager::Instance().CreateLogOption("SYNCHRONY");

	char input;
	do
	{
		std::string menuPrompt = "";
		menuPrompt += "1 Server\n";
		menuPrompt += "2 Client\n";
		menuPrompt += "3 Server & Client\n";
		menuPrompt += "Q Close Program\n";
		menuPrompt += "Enter Your Choise: ";

		logOption_->LogMessage(LogLevel::Log_Prompt, menuPrompt);

		cin >> input;
		input = toupper(input);

		if (input >= '1' && input <= '3')
			return input;
		else if (input == 'Q')
			break;
		else 
			logOption_->LogMessage(LogLevel::Log_Prompt, "You must enter one of the choices bellow:");

	} while (true);
	return 0;
}
void server()
{
	Server server;
	server.start();
	logOption_->LogMessage(LogLevel::Log_Info, "Server Started");
	
	while (server.getIsRunning()) {
		this_thread::sleep_for(std::chrono::milliseconds(1000));
	}

	server.stop();
	logOption_->LogMessage(LogLevel::Log_Info, "Server Started");

}
void client()
{
	HobbitClient hobbitClient;
	if (hobbitClient.start())
		return;
	logOption_->LogMessage(LogLevel::Log_Info, "Client Started");

	while (hobbitClient.isRunning()) {
		this_thread::sleep_for(std::chrono::milliseconds(2000));
	}

	hobbitClient.stop();
	logOption_->LogMessage(LogLevel::Log_Info, "Client Stoped");
}
void serverClient()
{
	Server server;
	server.start();
	logOption_->LogMessage(LogLevel::Log_Info, "Server Started");

	HobbitClient hobbitClient;
	string myIp = getLocalIPv4Address();
	if (hobbitClient.start(myIp))
		return;
	logOption_->LogMessage(LogLevel::Log_Info, "Client Started");

	while (hobbitClient.isRunning()) {
		this_thread::sleep_for(std::chrono::milliseconds(2000));
	}
	hobbitClient.stop();
	logOption_->LogMessage(LogLevel::Log_Info, "Client Stoped");
	server.stop();
	logOption_->LogMessage(LogLevel::Log_Info, "Server Stoped");
}

int main()
{
	do
	{
		switch (mainMenu())
		{
		case '0':
			logOption_->LogMessage(LogLevel::Log_Info, "End program");
			break;
		case '1':
			server();
			break;
		case '2':
			client();
			break;
		case '3':
			serverClient();
			break;
		default:
			logOption_->LogMessage(LogLevel::Log_Error, "This Menu Option was not available");
			break;
		}
	} while (true);
	return 0;
}