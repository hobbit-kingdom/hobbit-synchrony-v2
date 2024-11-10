#include "NPC.h"
HobbitProcessAnalyzer* NPC::hobbitProcessAnalyzer = nullptr;
//static member functions
void NPC::setHobbitProcessAnalyzer(HobbitProcessAnalyzer* newHobbitProcessAnalyzer)
{
	hobbitProcessAnalyzer = newHobbitProcessAnalyzer;
}

// Constructors
NPC::NPC() : logOption_(LogManager::Instance().CreateLogOption("NPC")) {

}
void NPC::setNCP(uint64_t GUID)
{
	guid = GUID;
	// Constructor message
	logOption_->LogMessage(LogLevel::Log_Debug, "Create NPC");
	logOption_->increaseDepth();
	//hobbitProcessAnalyzer->updateObjectStackAddress();

	// read the pointers of instances
	setObjectPtrByGUID(GUID);

	if (objectAddress == 0)
	{
		logOption_->LogMessage(LogLevel::Log_Warning, "Object Address set to 0");
		return;
	}

	setPositionXPtr();
	setRotationYPtr();
	setAnimationPtr();
	float a = getHealth();

	logOption_->decreaseDepth();
}

// Returns object pointer
uint32_t NPC::getObjectPtr() {
	return objectAddress;
}

// writes new GUID 
// modifies game file
void NPC::setGUID(uint32_t newGUID)
{
	uint32_t ObjectPtr = getObjectPtr();
	hobbitProcessAnalyzer->writeData(ObjectPtr, newGUID);
}
uint64_t NPC::getGUID()
{
	uint32_t ObjectPtr = getObjectPtr();
	return hobbitProcessAnalyzer->readData<uint64_t>(ObjectPtr + 0x8);
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
	uint32_t ObjectPtr = getObjectPtr();

	hobbitProcessAnalyzer->writeData(ObjectPtr + 0x290, newHealth);
}
float NPC::getHealth() {
	uint32_t ObjectPtr = getObjectPtr();
	 return hobbitProcessAnalyzer->readData<float>(ObjectPtr + 0x290);
}


void NPC::setAnimation(uint32_t newAnimation)
{
	hobbitProcessAnalyzer->writeData(animationAddress, newAnimation);
}
uint32_t NPC::getAnimation() {
	return hobbitProcessAnalyzer->readData<uint32_t>(animationAddress);
}
void NPC::setAnimFrames(float newAnimFrame, float newLastAnimFrame)
{
	hobbitProcessAnalyzer->writeData(animationAddress + 0x8, newAnimFrame);
	hobbitProcessAnalyzer->writeData(animationAddress + 0x14, newLastAnimFrame);
}

void NPC::setWeapon(uint32_t newWeapon)
{

	uint32_t ObjectPtr = getObjectPtr();
	hobbitProcessAnalyzer->writeData(ObjectPtr + 0x260, newWeapon);
}
//private

// Sets objects pointer of the NPC
void NPC::setObjectPtrByGUID(uint64_t guid)
{
	objectAddress = hobbitProcessAnalyzer->findGameObjByGUID(guid);
	objectPointer = hobbitProcessAnalyzer->findGameObjStackByPtrGUID(guid);

	//hex
	logOption_->LogMessage(LogLevel::Log_Debug, "ObjPointer: ", objectAddress);
	logOption_->LogMessage(LogLevel::Log_Debug, "ObjAddress: ", objectAddress);
}
// Sets position X pointers of the NPC 
void NPC::setPositionXPtr()
{
	// remove all stored pointers
	positionXAddress.clear();

	// store object pointer
	uint32_t ObjectPtr = getObjectPtr();

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
		logOption_->LogMessage(LogLevel::Log_Debug, "Health:", getHealth());
	}
	animationAddress = 0x8 + animAdd4;
	positionXAddress.push_back(-0xC4 + animationAddress);

	// Display the position X pointers Data
	for (uint32_t posxAdd : positionXAddress)
	{
		//hex
		logOption_->LogMessage(LogLevel::Log_Debug, "Position X Address:", posxAdd);
		logOption_->LogMessage(LogLevel::Log_Debug, "Position X:", hobbitProcessAnalyzer->readData<float>(posxAdd));
	}
}
// Sets rotation Y pointer of the NPC 
void NPC::setRotationYPtr()
{
	// store object pointer
	uint32_t ObjectPtr = getObjectPtr();

	// set rotation Y pointer
	rotationYAddress = 0x64 + 0x8 + ObjectPtr;

	// Display the rotation Y pointer Data
	//hex
	logOption_->LogMessage(LogLevel::Log_Debug, "Rotation Y Address:", rotationYAddress);
	logOption_->LogMessage(LogLevel::Log_Debug, "Rotation Y:", hobbitProcessAnalyzer->readData<float>(rotationYAddress));
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
	//hex
	logOption_->LogMessage(LogLevel::Log_Debug, "Animation Address:", animationAddress);
	logOption_->LogMessage(LogLevel::Log_Debug, "Animation:", hobbitProcessAnalyzer->readData<uint32_t>(animationAddress));
}