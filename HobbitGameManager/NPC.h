#pragma once
#include <vector>
#include "HobbitProcessAnalyzer.h"
#include <windows.h>
class NPC
{
public:

	// Constructors
	NPC();
	void setNCP(uint64_t GUID);
	static void setHobbitProcessAnalyzer(HobbitProcessAnalyzer* newHobbitProcessAnalyzer);
	// Returns object pointer
	uint32_t getObjectPtr();

	// writes new GUID 
	// modifies game file
	void setGUID(uint32_t newGUID);
	uint64_t getGUID();

	// writes new positionX 
	// modifies game file
	void setPositionX(uint32_t newPosition);
	void setPositionX(float newPosition);
	// writes new positionY
	// modifies game file
	void setPositionY(uint32_t newPosition);
	void setPositionY(float newPosition);
	// writes new positionZ 
	// modifies game file
	void setPositionZ(uint32_t newPosition);
	void setPositionZ(float newPosition);
	// writes new Position 
	// modifies game file
	void setPosition(uint32_t newPositionX, uint32_t newPositionY, uint32_t newPositionZ);
	void setPosition(float newPositionX, float newPositionY, float newPositionZ);

	// writes new GUID 
	// modifies game filen
	void setRotationY(uint32_t newRotation);
	void setRotationY(float newRotation);

	void setHealth(float newHealth);
	float getHealth();


	void setAnimation(uint32_t newAnimation);
	uint32_t getAnimation();
	void setAnimFrames(float newAnimFrame, float newLastAnimFrame);

	void setWeapon(uint32_t newWeapon);

	static HobbitProcessAnalyzer* hobbitProcessAnalyzer;
private:
	// Pointers
	uint32_t objectAddress = 0;					// Object pointer
	std::vector<uint32_t> positionXAddress;		// Position X pointer
	uint32_t rotationYAddress = 0;				// Rotation Y pointer
	uint32_t animationAddress = 0;				// Animation pointer

	// GUID of object
	uint64_t guid = 0;
	// Sets objects pointer of the NPC
	void setObjectPtrByGUID(uint64_t guid);
	// Sets position X pointers of the NPC 
	void setPositionXPtr();
	// Sets rotation Y pointer of the NPC 
	void setRotationYPtr();
	// Sets animation pointer of the NPC
	void setAnimationPtr();
};

