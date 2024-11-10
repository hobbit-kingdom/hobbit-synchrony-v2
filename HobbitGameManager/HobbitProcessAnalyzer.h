#pragma once
#pragma warning(push)
#pragma warning(disable : 4312)
#pragma warning(disable : 4267)
#include <mutex>
#include <iomanip>
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
	HobbitProcessAnalyzer() : logOption_(LogManager::Instance().CreateLogOption("HOBBIT PROC ANALYZ"))
	{
		LogManager::Instance().MoveLogOption("PROC ANALYZ WRAP", "HOBBIT PROC ANALYZ");
	}
	void updatePtrToProcess()
	{
		hobbitProcess = getProcess("Meridian.exe");
	}

	using ProcessAnalyzerTypeWrapped::readData;
	using ProcessAnalyzerTypeWrapped::writeData;
	using ProcessAnalyzerTypeWrapped::searchProcessMemory;

	bool isProcessSet() {
		if (hobbitProcess == nullptr)
			std::cerr << "ERROR: Hobbit Process is NOT set" << std::endl;

		return (hobbitProcess != nullptr);
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

	template <typename T>
	T readData(uint32_t address)
	{
		if (!isProcessSet()) return 0;
		return convertToType<T>(ProcessAnalyzer::readData(hobbitProcess, (LPVOID)address, sizeof(T)));
	}
	template <typename T>
	std::vector<T> readData(uint32_t address, size_t byesSize)
	{
		if (!isProcessSet()) return std::vector<T>(byesSize);
		return ProcessAnalyzerTypeWrapped::readData(hobbitProcess, (LPVOID)address, byesSize);
	}

	
	template <typename T>
	void writeData(uint32_t address, T data)
	{
		if (!isProcessSet()) return;
		ProcessAnalyzerTypeWrapped::writeData(hobbitProcess, (LPVOID)address, data);
	}
	template <typename T>
	void writeData(uint32_t address, std::vector<T> data)
	{
		if (!isProcessSet()) return;
		ProcessAnalyzerTypeWrapped::writeData(hobbitProcess, (LPVOID)address, data);
	}


	void updateObjectStackAddress()
	{
		try {
			std::lock_guard<std::mutex> lock(objectStackMutex);
			objectStackAddress = readData<uint32_t>(hobbitProcess, reinterpret_cast<LPVOID>(0x0076F648));
			objectStackSize = readData<uint32_t>(hobbitProcess, reinterpret_cast<LPVOID>(0x0076F660));

		}
		catch (const std::runtime_error& e) {
			std::lock_guard<std::mutex> lock(objectStackMutex);
			objectStackAddress = 0;
			objectStackSize = 0;
			std::cerr << "ERROR: Failed to read Object Stack Address from memory address 0x0076F648. Exception: " << e.what() << std::endl;
		}
	}

	uint32_t findGameObjByGUID(uint64_t guid)
	{
		if (!isProcessSet()) return 0;

		std::lock_guard<std::mutex> lock(objectStackMutex);

		for (size_t offset = 0; offset < objectStackSize * OBJECT_PTR_SIZE; offset += OBJECT_PTR_SIZE){
			uint32_t objStackAddress = objectStackAddress + offset;
			uint32_t objAddrs = readData<uint32_t>(hobbitProcess, reinterpret_cast<LPVOID>(objStackAddress));
			if (objAddrs != 0)
			{
				uint32_t guidAddrs = objAddrs + 0x8;
				uint64_t objGUID = readData<uint64_t>(hobbitProcess, reinterpret_cast<LPVOID>(guidAddrs));
				if (objGUID == guid)
				{
					return objAddrs;
				}
			}
		}
		//hex
		logOption_->LogMessage(LogLevel::Log_Warning, "Couldn't find", guid, " GUID in the Game Object Stack");
		return 0;
	}

	template <typename T>
	uint32_t findGameObjByPattern(T pattern, uint32_t shift)
	{
		if (!isProcessSet()) return 0;

		std::lock_guard<std::mutex> lock(objectStackMutex);

		for (size_t offset = 0; offset < objectStackSize * OBJECT_PTR_SIZE; offset += OBJECT_PTR_SIZE) {
			uint32_t objStackAddress = objectStackAddress + offset;
			uint32_t objAddrs = readData<uint32_t>(hobbitProcess, reinterpret_cast<LPVOID>(objStackAddress));
			if (objAddrs != 0)
			{
				uint32_t patternAddrs = objAddrs + shift;
				T objPattern = readData<T>(hobbitProcess, reinterpret_cast<LPVOID>(patternAddrs));
				if (objPattern == pattern)
				{
					return objAddrs;
				}
			}
		}
		//hex
		logOption_->LogMessage(LogLevel::Log_Warning, "Couldn't find", pattern, " Pattern in the Game Object Stack");
		return 0;
	}
	template <typename T>
	uint32_t findGameObjByPattern(const std::vector<T>& pattern, uint32_t shift)
	{
		if (!isProcessSet()) return 0;

		std::lock_guard<std::mutex> lock(objectStackMutex);

		for (size_t offset = 0; offset < objectStackSize * OBJECT_PTR_SIZE; offset += OBJECT_PTR_SIZE)
		{
			uint32_t objStackAddress = objectStackAddress + offset;
			uint32_t objAddrs = readData<uint32_t>(hobbitProcess, reinterpret_cast<LPVOID>(objStackAddress));
			if (objAddrs != 0)
			{
				uint32_t patternAddrs = objAddrs + shift;
				std::vector<T> objPattern = readData(hobbitProcess, reinterpret_cast<LPVOID>(patternAddrs), pattern.size() * sizeof(T));
				if (memcmp(objPattern.data(), pattern.data(), pattern.size()) == 0)
				{
					return objAddrs;
				}
			}
		}

		std::string warning = "";
		for (T e : pattern) warning += e + "_";
		logOption_->LogMessage(LogLevel::Log_Warning, "Couldn't find", warning, " Pattern in the Game Object Stack");

		return 0;
	}
	
	template <typename T>
	std::vector<uint32_t> findAllGameObjByPattern(T pattern, uint32_t shift)
	{
		if (!isProcessSet()) return std::vector<uint32_t>(0);

		std::vector<uint32_t> gameObjs;

		std::lock_guard<std::mutex> lock(objectStackMutex);

		for (size_t offset = 0; offset < objectStackSize * OBJECT_PTR_SIZE; offset += OBJECT_PTR_SIZE)
		{
			uint32_t objStackAddress = objectStackAddress + offset;
			uint32_t objAddrs = readData<uint32_t>(hobbitProcess, reinterpret_cast<LPVOID>(objStackAddress));
			if (objAddrs != 0)
			{
				uint32_t patternAddrs = objAddrs + shift;
				T objPattern = readData<T>(hobbitProcess, reinterpret_cast<LPVOID>(patternAddrs));
				if (objPattern == pattern)
				{
					gameObjs.push_back(objAddrs);
				}
			}
		}

		if (gameObjs.size() == 0)
		{
			logOption_->LogMessage(LogLevel::Log_Warning, "Couldn't find", pattern, " Pattern in the Game Object Stack");
		}

		return gameObjs;
	}
	template <typename T>
	std::vector<uint32_t> findAllGameObjByPattern(const std::vector<T>& pattern, uint32_t shift)
	{
		if (!isProcessSet()) return std::vector<uint32_t>(0);

		std::vector<uint32_t> gameObjs;

		std::lock_guard<std::mutex> lock(objectStackMutex);

		for (size_t offset = 0; offset < objectStackSize * OBJECT_PTR_SIZE; offset += OBJECT_PTR_SIZE)
		{
			uint32_t objStackAddress = objectStackAddress + offset;
			uint32_t objAddrs = readData<uint32_t>(hobbitProcess, reinterpret_cast<LPVOID>(objStackAddress));
			if (objAddrs != 0)
			{
				uint32_t patternAddrs = objAddrs + shift;
				std::vector<T> objPattern = readData(hobbitProcess, reinterpret_cast<LPVOID>(patternAddrs), pattern.size() * sizeof(T));
				if (memcmp(objPattern.data(), pattern.data(), pattern.size()) == 0)
				{
					gameObjs.push_back(objAddrs);
				}
			}
		}

		if (gameObjs.size() == 0)
		{
			std::cout << "WARNING: Couldn't find ";
			for (T e : pattern) std::cout << e << "_";
			std::cout << " Pattern in the Game Object Stack" << std::endl;
		}

		return gameObjs;
	}
	
	
	template <typename T, typename P>
	std::vector<T> findReadAllGameObjByPattern(P pattern, uint32_t patternShift, uint32_t readShift)
	{

		if (!isProcessSet()) return std::vector<T>();
		std::vector<T>  gameObjs;

		std::lock_guard<std::mutex> lock(objectStackMutex);

		for (size_t offset = 0; offset < objectStackSize * OBJECT_PTR_SIZE; offset += OBJECT_PTR_SIZE) {
			uint32_t objStackAddress = objectStackAddress + offset;
			uint32_t objAddrs = readData<uint32_t>(hobbitProcess, reinterpret_cast<LPVOID>(objStackAddress));
			if (objAddrs != 0)
			{
				uint32_t patternAddrs = objAddrs + patternShift;
				P objPattern = readData<P>(hobbitProcess, reinterpret_cast<LPVOID>(patternAddrs));
				if (objPattern == pattern)
				{
					patternAddrs = objAddrs + readShift;
					T objPattern = readData<T>(hobbitProcess, reinterpret_cast<LPVOID>(patternAddrs));
					gameObjs.push_back(objPattern);
				}
			}
		}
		return gameObjs;
	}
	template <typename T, typename P>

	std::vector<uint32_t> getAllObjects() {
		if (!isProcessSet()) return std::vector<uint32_t>(0);

		std::vector<uint32_t> foundObjects;

		std::lock_guard<std::mutex> lock(objectStackMutex);

		for (size_t offset = 0; offset < objectStackSize * OBJECT_PTR_SIZE; offset += OBJECT_PTR_SIZE) {
			uint32_t objStackAddress = objectStackAddress + offset;
			uint32_t objAddrs = readData<uint32_t>(hobbitProcess, reinterpret_cast<LPVOID>(objStackAddress));
			if (objAddrs != 0)
			{
				foundObjects.push_back(objAddrs);
			}
		}

		return foundObjects;
	}


private:
	LogOption::Ptr logOption_;
	HANDLE hobbitProcess = 0;

	uint32_t objectStackSize = 0x0;
	const uint32_t OBJECT_PTR_SIZE = 0x14;
	uint32_t objectStackAddress = 0;

	std::mutex objectStackMutex;
};