
#include "HobbitMultiplayer.h"

using namespace std;

int HobbitMultiplayer::mainMenu()
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
int HobbitMultiplayer::startMenu()
{
	do
	{
		switch (mainMenu())
		{
		case '0':
			logOption_->LogMessage(LogLevel::Log_Info, "End program");
			break;
		case '1':
			startServer();
			break;
		case '2':
			startClient();
			break;
		case '3':
			sstartServerClient();
			break;
		default:
			logOption_->LogMessage(LogLevel::Log_Error, "This Menu Option was not available");
			break;
		}
	} while (true);
	return 0;
}

void HobbitMultiplayer::startServer()
{
	server.start();
	logOption_->LogMessage(LogLevel::Log_Info, "Server Started");

	while (server.getIsRunning()) {
		this_thread::sleep_for(std::chrono::milliseconds(1000));
	}

	server.stop();
	logOption_->LogMessage(LogLevel::Log_Info, "Server Started");

}
void HobbitMultiplayer::startClient()
{
	if (hobbitClient.start())
		return;
	logOption_->LogMessage(LogLevel::Log_Info, "Client Started");

	while (hobbitClient.isRunning()) {
		this_thread::sleep_for(std::chrono::milliseconds(2000));
	}

	hobbitClient.stop();
	logOption_->LogMessage(LogLevel::Log_Info, "Client Stoped");
}

void HobbitMultiplayer::sstartServerClient()
{
	server.start();
	logOption_->LogMessage(LogLevel::Log_Info, "Server Started");

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


