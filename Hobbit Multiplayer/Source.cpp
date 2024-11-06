#define DEBUG
#include <iostream>
#include <thread>
#include <atomic>
#include <optional> // Make sure to include this for std::optional

#include "HobbitClient.h"
#include "../ServerClient/Server.h"
#include "../ServerClient/IPv4.h"

//THIS #include <windows.h> MUST BE AFTER THE ../ServerClient AND ../HobbitGameManager
#include <windows.h>

using namespace std;

int mainMenu()
{

	char input;
	do
	{
		cout << "1 Server" << endl;
		cout << "2 Client" << endl;
		cout << "3 Server & Client" << endl;
		cout << "Q Close Program" << endl << endl;
		cout << "Enter Your Choise: ";

		cin >> input;
		input = toupper(input);

		if (input >= '1' && input <= '3')
			return input;
		else if (input == 'Q')
			break;
		else 
			cout << "You must enter one of the choices bellow:" << endl;

	} while (true);
	return 0;
}
void server()
{
	Server server;
	server.start();
	cout << "Server Started" << endl;
	cout << "Press Ctrl+X to stop the server..." << endl;
	
	while (server.getIsRunning()) {
		this_thread::sleep_for(std::chrono::milliseconds(1000));
	}

	server.stop();
	cout << "Server Stoped" << endl;

}
void client()
{
	HobbitClient hobbitClient;
	if (hobbitClient.start())
		return;
	cout << "Client Started" << endl;

	while (hobbitClient.isRunning()) {
		this_thread::sleep_for(std::chrono::milliseconds(2000));
	}

	hobbitClient.stop();
	cout << "Hobbit Client Stoped" << endl;
}
void serverClient()
{
	Server server;
	server.start();
	cout << "Server Started" << endl;

	HobbitClient hobbitClient;
	string myIp = getLocalIPv4Address();
	if (hobbitClient.start(myIp))
		return;
	cout << "Client Started" << endl;

	while (hobbitClient.isRunning()) {
		this_thread::sleep_for(std::chrono::milliseconds(2000));
	}
	hobbitClient.stop();
	cout << "Hobbit Client Stoped" << endl;
	server.stop();
	cout << "Server Stoped" << endl;
}

int main()
{
	do
	{
		switch (mainMenu())
		{
		case '0':
			cout << "End program" << endl;
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
			cout << "ERROR: This Menu Option was not available" << endl;
			break;
		}
	} while (true);
	return 0;
}