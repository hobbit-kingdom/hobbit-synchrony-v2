#pragma once
#ifdef _WIN32
#include <winsock2.h>   // Ensure winsock2.h is included before Windows.h
#include <Windows.h>
#include<memoryapi.h>
#include<TlHelp32.h>
#endif

#include<iostream>
#include<vector>

#include <cstdint> 
#include "ProcessAnalyzer.h"
class ProcessAnalyzerTypeWrapped : protected ProcessAnalyzer {

public:

	using ProcessAnalyzer::readData;
	using ProcessAnalyzer::writeData;
	using ProcessAnalyzer::getProcess;
	using ProcessAnalyzer::searchProcessMemory;

	float hexToFloat(uint32_t number) {
		// Use a union to safely convert uint32_t to float
		union {
			uint32_t u;
			float f;
		} converter;

		converter.u = number;
		return converter.f;
	}

	HANDLE getProcess(std::string processName)
	{
		getProcess(processName.c_str());
	}
	
	template <typename T>
	void writeData(HANDLE process, LPVOID address, T data)
	{
		ProcessAnalyzer::writeData(process, address, convertToUint8Vector(data));
	}
	template <typename T>
	void writeData(HANDLE process, LPVOID address, std::vector<T> data)
	{
		ProcessAnalyzer::writeData(process, address, convertToUint8Vector(data));
	}

	template <typename T>
	T readData(HANDLE process, LPVOID address)
	{
		return convertToType<T>(ProcessAnalyzer::readData(process, address, sizeof(T)));
	}
	template <typename T>
	std::vector<T> readData(HANDLE process, LPVOID address, size_t size)
	{
		return convertToVector<T>(ProcessAnalyzer::readData(process, address, size * sizeof(T)));
	}

	template <typename T>
	std::vector<uint32_t> searchProcessMemory(HANDLE process, T pattern)
	{
		return searchProcessMemory(process, convertToUint8Vector(pattern));
	}
	template <typename T>
	std::vector<uint32_t> searchProcessMemory(HANDLE process, const std::vector<T>& pattern)
	{
		return searchProcessMemory(process, convertToUint8Vector(pattern));
	}

	template <typename T>
	std::vector<uint8_t> convertToUint8Vector(const T& value) {
		static_assert(std::is_trivially_copyable<T>::value, "Type T must be trivially copyable");

		std::vector<uint8_t> byteVector(sizeof(T));
		std::memcpy(byteVector.data(), &value, sizeof(T));

		return byteVector;
	}
	template <typename T>
	std::vector<uint8_t> convertToUint8Vector(const std::vector<T>& vec, bool inputBigEndian = true) {
		static_assert(std::is_trivially_copyable<T>::value, "Type T must be trivially copyable");

		std::vector<uint8_t> byteVector;
		byteVector.reserve(vec.size() * sizeof(T)); // Reserve space for the bytes

		for (const auto& num : vec) {
			T value = num;

			// Swap bytes if the input is in big-endian format
			if (inputBigEndian) {
				swapBytes(value);
			}

			uint8_t bytes[sizeof(T)];
			std::memcpy(bytes, &value, sizeof(T));
			byteVector.insert(byteVector.end(), bytes, bytes + sizeof(T)); // Append the bytes
		}

		return byteVector;
	}


	template <typename T>
	std::vector<T> convertToVector(const std::vector<uint8_t>& data) {
		if (T == uint8_t)
			return data;

		size_t numElements = data.size() / sizeof(T);
		std::vector<T> result(numElements);

		std::memcpy(result.data(), data.data(), data.size());

		return result;
	}

	template <typename T>
	T convertToType(const std::vector<uint8_t>& data, bool targetLittleEndian = true) {
		if (data.size() < sizeof(T)) {
			throw std::runtime_error("Not enough data to convert to type T");
		}

		T result;
		std::memcpy(&result, data.data(), sizeof(T));

		// Check system endianness
		const bool isLittleEndian = []() {
			uint16_t num = 1;
			return *(reinterpret_cast<uint8_t*>(&num)) == 1;
			}();

		// If the target endianness is different from the system's endianness, swap the bytes
		if (isLittleEndian != targetLittleEndian) {
			swapBytes(result);
		}

		return result;
	}

	template <typename T>
	void swapBytes(T& value) {
		uint8_t* bytePtr = reinterpret_cast<uint8_t*>(&value);
		std::reverse(bytePtr, bytePtr + sizeof(T));
	}
};