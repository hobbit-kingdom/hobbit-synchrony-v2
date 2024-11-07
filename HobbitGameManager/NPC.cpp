#include "NPC.h"
HobbitProcessAnalyzer* NPC::hobbitProcessAnalyzer = nullptr;
//static member functions
void NPC::setHobbitProcessAnalyzer(HobbitProcessAnalyzer* newHobbitProcessAnalyzer)
{
	hobbitProcessAnalyzer = newHobbitProcessAnalyzer;
}

//public

// Constructors
NPC::NPC() {
}
void NPC::setNCP(uint64_t GUID)
{
	guid = GUID;
	// Constructor message
	std::cout << "~CreateNPC" << std::endl;
	hobbitProcessAnalyzer->updateObjectStackAddress();
	// read the pointers of instances
	setObjectPtrByGUID(GUID);
	setPositionXPtr();
	setRotationYPtr();
	setAnimationPtr();

	// end Constructor message
	std::cout << std::endl;
}

// Returns object pointer
uint32_t NPC::getObjectPtr() {
	return objectAddress;
}

// writes new GUID 
// modifies game file
void NPC::setGUID(uint32_t newGUID)
{
	hobbitProcessAnalyzer->writeData(objectAddress, newGUID);
}
uint64_t NPC::getGUID()
{
	return hobbitProcessAnalyzer->readData<uint64_t>(objectAddress + 0x8);
}

// writes new positionX 
// modifies game file
void NPC::setPositionX(uint32_t newPosition)
{
	for (uint32_t posXadd : positionXAddress)
	{
		hobbitProcessAnalyzer->writeData(posXadd, newPosition);
	}
}
void NPC::setPositionX(float newPosition)
{
	for (uint32_t posXadd : positionXAddress)
	{
		hobbitProcessAnalyzer->writeData(posXadd, newPosition);
	}
}

// writes new positionY
// modifies game file
void NPC::setPositionY(uint32_t newPosition)
{
	for (uint32_t posXadd : positionXAddress)
	{
		hobbitProcessAnalyzer->writeData(0x4 + posXadd, newPosition);
	}
}
void NPC::setPositionY(float newPosition)
{
	for (uint32_t posXadd : positionXAddress)
	{
		hobbitProcessAnalyzer->writeData(0x4 + posXadd, newPosition);
	}
}

// writes new positionZ 
// modifies game file
void NPC::setPositionZ(uint32_t newPosition)
{
	for (uint32_t posXadd : positionXAddress)
	{
		hobbitProcessAnalyzer->writeData(0x8 + posXadd, newPosition);
	}
}
void NPC::setPositionZ(float newPosition)
{
	for (uint32_t posXadd : positionXAddress)
	{
		hobbitProcessAnalyzer->writeData(0x8 + posXadd, newPosition);
	}
}

// writes new Position 
// modifies game file
void NPC::setPosition(uint32_t newPositionX, uint32_t newPositionY, uint32_t newPositionZ)
{
	setPositionX(newPositionX);
	setPositionY(newPositionY);
	setPositionZ(newPositionZ);
}
void NPC::setPosition(float newPositionX, float newPositionY, float newPositionZ)
{
	setPositionX(newPositionX);
	setPositionY(newPositionY);
	setPositionZ(newPositionZ);
}

// writes new GUID 
// modifies game filen
void NPC::setRotationY(uint32_t newRotation)
{
	hobbitProcessAnalyzer->writeData(rotationYAddress, newRotation);
}
void NPC::setRotationY(float newRotation)
{
	hobbitProcessAnalyzer->writeData(rotationYAddress, newRotation);
}


void NPC::setHealth(float newHealth) {
	//TO DO
	// Set correct shfit from the heatlh
	hobbitProcessAnalyzer->writeData(objectAddress + 0x270 + 0x5 * 0x4, newHealth);
}
float NPC::getHealth() {
	// Set correct shfit from the heatlh
	 return hobbitProcessAnalyzer->readData<uint32_t>(objectAddress + 0x270 + 0x5*0x4);
}


void NPC::setAnimation(uint32_t newAnimation)
{
	hobbitProcessAnalyzer->writeData(animationAddress, newAnimation);
}
void NPC::setAnimFrames(float newAnimFrame, float newLastAnimFrame)
{
	hobbitProcessAnalyzer->writeData(animationAddress + 0x8, newAnimFrame);
	hobbitProcessAnalyzer->writeData(animationAddress + 0x14, newLastAnimFrame);
}


//private

// Sets objects pointer of the NPC
void NPC::setObjectPtrByGUID(uint64_t guid)
{
	objectAddress = hobbitProcessAnalyzer->findGameObjByGUID(guid);

	// Display new ObjAddress
	std::cout << std::hex;
	std::cout << "~ObjectPtr: " << objectAddress;
	std::cout << std::dec << std::endl;
}
// Sets position X pointers of the NPC 
void NPC::setPositionXPtr()
{
	// remove all stored pointers
	positionXAddress.clear();

	// store object pointer
	uint32_t ObjectPtr = getObjectPtr();

	//0DEB3EBC
	// 0x0deb3ea8 + 0xC + 0x8
	// set current position pointer
	positionXAddress.push_back(0xC + 0x8 + ObjectPtr);

	// set root position X pointer
	positionXAddress.push_back(0x18 + 0x8 + ObjectPtr);

	// set the animation position X pointer
	uint32_t animAdd1 = getObjectPtr();
	uint32_t animAdd2 = hobbitProcessAnalyzer->readData<uint32_t>(0x304 + animAdd1);
	uint32_t animAdd3 = hobbitProcessAnalyzer->readData<uint32_t>(0x50 + animAdd2);
	uint32_t animAdd4 = hobbitProcessAnalyzer->readData<uint32_t>(0x10C + animAdd3);
	if (animAdd4 == 0) 
	{
		std::cout << "Health: " << getHealth() << std::endl;
	}
	animationAddress = 0x8 + animAdd4;
	positionXAddress.push_back(-0xC4 + animationAddress);

	// Display the position X pointers Data
	std::cout << std::hex;
	for (uint32_t posxAdd : positionXAddress)
	{
		//dispplay the poistion Data
		std::cout << "~Position Data:" << std::endl;
		std::cout << "~posX: " << hobbitProcessAnalyzer->readData<float>(posxAdd) << std::endl;
		std::cout << "~posXAddress: " << posxAdd << std::endl;
	}
	std::cout << std::dec;
	std::cout << std::endl;
}
// Sets rotation Y pointer of the NPC 
void NPC::setRotationYPtr()
{
	// store object pointer
	uint32_t ObjectPtr = getObjectPtr();

	// set rotation Y pointer
	rotationYAddress = 0x64 + 0x8 + ObjectPtr;

	// Display the rotation Y pointer Data
	std::cout << std::hex;
	std::cout << "~Rotation Data:" << std::endl;
	std::cout << "~rotY: " << hobbitProcessAnalyzer->readData<float>(rotationYAddress) << std::endl;
	std::cout << "~rotYAddress: " << rotationYAddress << std::endl;
	std::cout << std::endl;
	std::cout << std::dec;
}
// Sets animation pointer of the NPC
void NPC::setAnimationPtr()
{
	// set animation pointer
	uint32_t animAdd1 = getObjectPtr();
	uint32_t animAdd2 = hobbitProcessAnalyzer->readData<uint32_t>(0x304 + animAdd1);
	uint32_t animAdd3 = hobbitProcessAnalyzer->readData<uint32_t>(0x50 + animAdd2);
	uint32_t animAdd4 = hobbitProcessAnalyzer->readData<uint32_t>(0x10C + animAdd3);
	animationAddress = 0x8 + animAdd4;


	// Display the animation pointer Data
	std::cout << std::hex;
	std::cout << "anim: " << hobbitProcessAnalyzer->readData<uint32_t>(animationAddress) << std::endl;
	std::cout << "animAddress: " << animationAddress << std::endl;
	std::cout << std::endl;
}