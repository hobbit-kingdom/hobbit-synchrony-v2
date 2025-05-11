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
#include "../LogSystem/LogManager.h"

// Player classes
class ConnectedPlayer {
	HobbitProcessAnalyzer* hobbitProcessAnalyzer;
	uint32_t animation;
	float animFrame, lastAnimFrame;
	Vector3 position, rotation;

	uint32_t level;
	uint32_t levels;
	int8_t weapon;

	uint8_t hostLevel;


	LogOption::Ptr logOption_;

public:
	ConnectedPlayer() : logOption_(LogManager::Instance().CreateLogOption("CONNECTED PLAYER"))
	{

	}
	void setHobbitProcessAnalyzer(HobbitGameManager& initialHobbitGameManager)
	{
		hobbitProcessAnalyzer = initialHobbitGameManager.getHobbitProcessAnalyzer();
		npc.setHobbitProcessAnalyzer(hobbitProcessAnalyzer);
	}
	int id = -1;
	NPC npc;

	void readConectedPlayerSnap(std::queue<uint8_t>& gameData) {
		hostLevel = convertQueueToType<uint8_t>(gameData);
		animation = convertQueueToType<uint32_t>(gameData);
		animFrame = convertQueueToType<float>(gameData);
		lastAnimFrame = convertQueueToType<float>(gameData);
		position.x = convertQueueToType<float>(gameData);
		position.y = convertQueueToType<float>(gameData);
		position.z = convertQueueToType<float>(gameData);
		rotation.y = convertQueueToType<float>(gameData);
		weapon = convertQueueToType<int8_t>(gameData);
	}

	void processPlayer(uint8_t myId)
	{
		if (id == -1 || id == myId)
			return;

		// Display the data
		logOption_->LogMessage(LogLevel::Log_Debug, "Process Msg");
		logOption_->increaseDepth();
		logOption_->LogMessage(LogLevel::Log_Debug, "Anim", animation);
		logOption_->LogMessage(LogLevel::Log_Debug, "Pos", position.x, position.y, position.z);
		logOption_->LogMessage(LogLevel::Log_Debug, "RotY", rotation.y);
		logOption_->LogMessage(LogLevel::Log_Debug, "Weapon", int(weapon));
		logOption_->decreaseDepth();

		if (hostLevel != hobbitProcessAnalyzer->readData<uint8_t>(0x00762B5C))
		{
			logOption_->setColor("RED");
			logOption_->LogMessage(LogLevel::Log_Debug, "No Synchrone");
			logOption_->resetColor();
			return;
		}

		//set position, rotation, and animation
		npc.setPositionX(position.x);
		npc.setPositionY(position.y);
		npc.setPositionZ(position.z);
		npc.setRotationY(rotation.y);
		if (animation != npc.getAnimation())
		{
			npc.setAnimation(animation);
			npc.setAnimFrames(animFrame, lastAnimFrame);
		}


		uint64_t guidNone = 0x0;
		if (weapon != -1)
		{
			//NONE
			if (weapon == 2)
				guidNone = 0x0D8AD910E885100D;
			//STING
			else if (weapon == 0)
				guidNone = 0x0D8AD910E885100B;
			//STAFF
			else if (weapon == 1)
				guidNone = 0x0D8AD910E885100A;
			//STONE
			else if (weapon == 3)
				guidNone = 0x0D8AD910E885100C;

			uint32_t addrsGuidNone = hobbitProcessAnalyzer->findGameObjByGUID(guidNone);
			if (addrsGuidNone)
				npc.setWeapon(hobbitProcessAnalyzer->readData<uint32_t>(addrsGuidNone + 0x260));
		}

	}


	std::vector<BaseMessage> write() {
		//In case you need to send some state from the npc bilbo (which is unlickely) 
		return std::vector<BaseMessage>();
	}

	void clear() { id = -1; }
};