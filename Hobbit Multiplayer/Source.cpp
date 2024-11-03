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

std::atomic<bool> ctrlXPressed(false);

void checkForCtrlX() {
	std::cout << "Press Ctrl + X to exit." << std::endl;

	while (true) {
		// Check if Ctrl is pressed
		if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
			// Check if X is pressed
			if (GetAsyncKeyState('X') & 0x8000) {
				ctrlXPressed.store(true); // Set the flag to true
				break; // Exit the loop if Ctrl + X is pressed
			}
		}
		Sleep(100); // Sleep for a short time to avoid busy waiting
	}
}

int mainMenu()
{

	char input;
	do
	{
		cout << "1 Server" << endl;
		cout << "2 Client" << endl;
		cout << "3 Server & Client" << endl;
		cout << "Ctrl + X Close Program" << endl << endl;
		cout << "Enter Your Choise: ";

		cin >> input;
		if (ctrlXPressed) break;

		if (input >= '1' && input <= '3') 
			return input;
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
	
	while (!ctrlXPressed) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
	cout << "Press Ctrl + X to stop the client ..." << endl;

	while (!ctrlXPressed) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
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


	cout << "Press Ctrl + X to stop the client ..." << endl;

	while (!ctrlXPressed) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	server.stop();
	cout << "Server Stoped" << endl;
	hobbitClient.stop();
	cout << "Hobbit Client Stoped" << endl;
}

int main()
{
	// Start the Ctrl + X checking function in a separate thread
	std::thread ctrlXThread(checkForCtrlX);
	do
	{
		if (ctrlXPressed) break;
		switch (mainMenu())
		{
		case '0':
			cout << "end program" << endl;
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
	if(ctrlXThread.joinable())
		ctrlXThread.join();
	return 0;
}