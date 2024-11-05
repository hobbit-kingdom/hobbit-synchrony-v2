#pragma once
#pragma warning(push)
#pragma warning(disable : 4312)
#pragma warning(disable : 4267)
#include <mutex>
#ifdef _WIN32
#include <winsock2.h>   // Ensure winsock2.h is included before Windows.h
#include <Windows.h>
#endif
#include<unordered_map>
#include <iomanip>
#include"ProcessAnalyzerTypeWrapped.h"
class HobbitProcessAnalyzer : public ProcessAnalyzerTypeWrapped
{
public:
	HobbitProcessAnalyzer()
	{
		
	}
	void updatePtrToProcess()
	{
		hobbitProcess = getProcess("Meridian.exe");
	}
	using ProcessAnalyzer::readData;
	using ProcessAnalyzerTypeWrapped::readData;
	using ProcessAnalyzer::writeData;
	using ProcessAnalyzerTypeWrapped::writeData;
	using ProcessAnalyzerTypeWrapped::searchProcessMemory;


	std::vector<uint8_t> readData(uint32_t address, size_t byesSize)
	{
		return ProcessAnalyzer::readData(hobbitProcess, (LPVOID)address, byesSize);
	}

	template <typename T>
	T readData(uint32_t address, size_t byesSize)
	{
		return convertToType<T>(ProcessAnalyzer::readData(hobbitProcess, (LPVOID)address, byesSize));
	}

	// Write a single data element of type T to the specified memory address in hobbitProcess
	template <typename T>
	void writeData(uint32_t address, const T& data)
	{
		if (hobbitProcess == nullptr)
		{
			std::cerr << "ERROR: Hobbit Process is NOT set" << std::endl;
			return;
		}

		// Convert the data to a vector of bytes for writing
		std::vector<uint8_t> byteData = convertToUint8Vector(data);

		// Use the base class writeData function to perform the memory write
		ProcessAnalyzer::writeData(hobbitProcess, reinterpret_cast<LPVOID>(address), byteData);
		return;
	}
	// Write a vector of data to the specified memory address in hobbitProcess
	template <typename T>
	void writeData(uint32_t address, const std::vector<T>& dataVec)
	{
		if (hobbitProcess == nullptr)
		{
			std::cerr << "ERROR: Hobbit Process is NOT set" << std::endl;
			return false;
		}

		// Convert the vector of data elements to a vector of bytes
		std::vector<uint8_t> byteData = convertToUint8Vector(dataVec);

		// Write the byte vector to the target address
		ProcessAnalyzer::writeData(hobbitProcess, reinterpret_cast<LPVOID>(address), byteData);
	}


	void updateObjectStackAddress()
	{
		try {
			std::lock_guard<std::mutex> lock(objectStackMutex);
			objectStackAddress = readData<uint32_t>(hobbitProcess, reinterpret_cast<LPVOID>(0x0076F648), 4);
			objectStackSize = readData<uint32_t>(hobbitProcess, reinterpret_cast<LPVOID>(0x0076F650), 4);
		}
		catch (const std::runtime_error& e) {
			std::lock_guard<std::mutex> lock(objectStackMutex);
			objectStackAddress = 0;
			std::cerr << "ERROR: Failed to read Object Stack Address from memory address 0x0076F648. Exception: " << e.what() << std::endl;
		}
	}

	uint32_t findGameObjByGUID(uint64_t guid)
	{
		if (hobbitProcess == 0)
		{
			std::cout << "ERROR: Hobbit Process is NOT set" << std::endl;
			return 0;
		}

		static const size_t jumpSize = 0x14;
		std::lock_guard<std::mutex> lock(objectStackMutex);
		size_t offset = 0;
		while(true){
		//for (size_t offset = 0; offset <= objectStackSize * jumpSize; offset += jumpSize)
			uint32_t objStackAddress = objectStackAddress + offset;
			uint32_t objAddrs = readData<uint32_t>(hobbitProcess, reinterpret_cast<LPVOID>(objStackAddress), 4);
			if (objAddrs != 0)
			{
				uint32_t guidAddrs = objAddrs + 0x8;
				uint64_t objGUID = readData<uint64_t>(hobbitProcess, reinterpret_cast<LPVOID>(guidAddrs), 8);
				if (objGUID == guid)
				{
					std::cout << "Found GUID at ObjectAddres: " << offset / 0x14 << std::endl;
					std::cout << "Found GUID at objectStackSize: " << objectStackSize << std::endl;
					return objAddrs;
				}
			}
			offset += jumpSize;
		}

		std::cout << "WARNING: Couldn't find " << guid << " GUID in the Game Object Stack" << std::endl;
		return 0;
	}

	uint32_t findGameObjByPattern(const std::vector<uint8_t>& data, size_t dataSize, uint32_t shift)
	{
		if (hobbitProcess == 0)
		{
			std::cout << "ERROR: Hobbit Process is NOT set" << std::endl;
			return 0;
		}

		static const size_t jumpSize = 0x14;
		std::lock_guard<std::mutex> lock(objectStackMutex);

		for (size_t offset = 0; offset <= objectStackSize * jumpSize; offset += jumpSize)
		{
			uint32_t objStackAddress = objectStackAddress + offset;
			uint32_t objAddrs = readData<uint32_t>(hobbitProcess, reinterpret_cast<LPVOID>(objStackAddress), 4);
			if (objAddrs != 0)
			{
				uint32_t patternAddrs = objAddrs + shift;
				std::vector<uint8_t> objPattern = readData(hobbitProcess, reinterpret_cast<LPVOID>(patternAddrs), dataSize);
				if (memcmp(objPattern.data(), data.data(), dataSize) == 0)
				{
					return objAddrs;
				}
			}
		}

		std::cout << "WARNING: Couldn't find " << data[0] << " GUID in the Game Object Stack" << std::endl;
		return 0;
	}

	std::vector<uint32_t> findAllGameObjByPattern(const std::vector<uint8_t>& data, size_t dataSize, uint32_t shift)
	{
		if (hobbitProcess == nullptr)
		{
			std::cout << "ERROR: Hobbit Process is NOT set" << std::endl;
			return std::vector<uint32_t>();
		}

		std::vector<uint32_t> gameObjs;

		static const size_t jumpSize = 0x14;
		std::lock_guard<std::mutex> lock(objectStackMutex);

		for (size_t offset = 0; offset <= objectStackSize * jumpSize; offset += jumpSize)
		{
			uint32_t objStackAddress = objectStackAddress + offset;
			uint32_t objAddrs = readData<uint32_t>(hobbitProcess, reinterpret_cast<LPVOID>(objStackAddress), 4);
			if (objAddrs != 0)
			{
				uint32_t patternAddrs = objAddrs + shift;
				std::vector<uint8_t> objPattern = readData(hobbitProcess, reinterpret_cast<LPVOID>(patternAddrs), dataSize);
				if (memcmp(objPattern.data(), data.data(), dataSize) == 0)
				{
					gameObjs.push_back(objStackAddress);
				}
			}
		}

		return gameObjs;
	}
	std::unordered_map<uint32_t, std::vector<uint8_t>> readAllGameObjByPattern(size_t dataSize, uint32_t shift)
	{
		if (hobbitProcess == nullptr)
		{
			std::cout << "ERROR: Hobbit Process is NOT set" << std::endl;
			return std::unordered_map<uint32_t, std::vector<uint8_t>>();
		}
		std::unordered_map<uint32_t, std::vector<uint8_t>>  gameObjs(0);

		static const size_t jumpSize = 0x14;
		std::lock_guard<std::mutex> lock(objectStackMutex);

		for (size_t offset = 0; offset <= objectStackSize * jumpSize; offset += jumpSize)
		{
			uint32_t objStackAddress = objectStackAddress + offset;
			uint32_t objAddrs = readData<uint32_t>(hobbitProcess, reinterpret_cast<LPVOID>(objStackAddress), 4);
			if (objAddrs != 0)
			{
				uint32_t patternAddrs = objAddrs + shift;
				gameObjs[objStackAddress] = readData(hobbitProcess, reinterpret_cast<LPVOID>(patternAddrs), dataSize);
			}
		}

		return gameObjs;
	}

	std::vector<uint32_t> getAllObjects() {
		if (hobbitProcess == nullptr)
		{
			std::cout << "ERROR: Hobbit Process is NOT set" << std::endl;
			return std::vector<uint32_t>();
		}

		std::vector<uint32_t> foundObjects;

		static const size_t jumpSize = 0x14;
		std::lock_guard<std::mutex> lock(objectStackMutex);

		for (size_t offset = 0; offset <= objectStackSize * jumpSize; offset += jumpSize) {
			uint32_t objStackAddress = objectStackAddress + offset;
			uint32_t objAddrs = readData<uint32_t>(hobbitProcess, reinterpret_cast<LPVOID>(objStackAddress), 4);
			if (objAddrs != 0)
			{
				foundObjects.push_back(objAddrs);
			}
		}

		return foundObjects;
	}

	bool isGameRunning()
	{
		return  getProcess("Meridian.exe") != nullptr;
	}


	template <typename T>
	std::vector<uint32_t> searchProcessMemory(T pattern)
	{
		return ProcessAnalyzerTypeWrapped::searchProcessMemory(hobbitProcess, convertToUint8Vector(pattern));
	}
	template <typename T>
	std::vector<uint32_t> searchProcessMemory(const std::vector<T>& pattern)
	{
		return ProcessAnalyzerTypeWrapped::searchProcessMemory(hobbitProcess, convertToUint8Vector(pattern));
	}

private:

	HANDLE hobbitProcess = 0;

	uint32_t objectStackSize = 0x0;
	uint32_t objectStackAddress = 0;

	std::mutex objectStackMutex;
};