#pragma once
#ifdef _WIN32
#include <winsock2.h>   // Ensure winsock2.h is included before Windows.h
#include <Windows.h>
#include <memoryapi.h>
#include <TlHelp32.h>
#include "../LogSystem/LogManager.h"
#endif

#include <iostream>
#include <vector>
#include <cstdint>

class ProcessAnalyzer
{
	LogOption::Ptr logOption_;

public:
	ProcessAnalyzer() : logOption_(LogManager::Instance().CreateLogOption("PROC ANALYZ"))
	{

	}
	HANDLE getProcess(const char* processName)
	{
		HANDLE process;
		int pid = 0;

		HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (snapshot == INVALID_HANDLE_VALUE)
		{
			logOption_->LogMessage(LogLevel::Log_Error, GetLastError());
			return nullptr;
		}

		PROCESSENTRY32 pe32;
		pe32.dwSize = sizeof(PROCESSENTRY32); // you need this as windows API may evole and have different size for ProcessEntry32

		if (Process32First(snapshot, &pe32))
		{
			do
			{
				WCHAR wProcessName[MAX_PATH];
				MultiByteToWideChar(CP_ACP, 0, processName, -1, wProcessName, MAX_PATH);

				if (wcscmp(pe32.szExeFile, wProcessName) == 0)
				{
					pid = pe32.th32ProcessID;
					break;
				}
			} while (Process32Next(snapshot, &pe32));
		}

		if (snapshot != NULL)
			CloseHandle(snapshot);

		if (pid == 0) {
			logOption_->LogMessage(LogLevel::Log_Warning, processName, " - Process Not Found");
			return nullptr;
		}

		HANDLE processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
		process = processHandle;
		return processHandle;
	}

	void writeData(HANDLE process, LPVOID address, std::vector<uint8_t> data)
	{
		if (process == NULL)
		{
			logOption_->LogMessage(LogLevel::Log_Error, process, "Process Not Specified");
			return;
		}

		SIZE_T dwSize = data.size();
		DWORD oldProtect;

		//change protection for the slelcted memory to read and write
		if (!VirtualProtectEx(process, address, dwSize, PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			logOption_->LogMessage(LogLevel::Log_Error, GetLastError());
			return;
		}

		//write the data
		if (!WriteProcessMemory(process, address, data.data(), dwSize, NULL))
		{
			logOption_->LogMessage(LogLevel::Log_Error, GetLastError());
		}

		//change protection to the previous state
		VirtualProtectEx(process, address, dwSize, oldProtect, NULL);
	}

	std::vector<uint8_t> readData(HANDLE process, LPVOID address, size_t bytesSize)
	{
		std::vector<uint8_t> data(bytesSize);
		if (process == NULL)
		{
			logOption_->LogMessage(LogLevel::Log_Error, "Process Not Specified");
			return data;
		}

		// Change memory protection to PAGE_READWRITE
		DWORD oldProtect;
		if (!VirtualProtectEx(process, address, bytesSize, PAGE_READWRITE, &oldProtect))
		{
			logOption_->LogMessage(LogLevel::Log_Error, "Could not change memory protection: ", GetLastError());
			return data;
		}

		// Read the memory
		if (!ReadProcessMemory(process, address, data.data(), bytesSize, NULL))
		{
			logOption_->LogMessage(LogLevel::Log_Error, "Could not read memory: ", GetLastError());
			return data;
		}

		// Restore the old protection
		VirtualProtectEx(process, address, bytesSize, oldProtect, &oldProtect);

		return data;
	}



	std::vector<uint32_t> searchProcessMemory(HANDLE process, const std::vector<uint8_t>& pattern) {
		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);

		MEMORY_BASIC_INFORMATION mbi;
		SIZE_T address = 0;
		std::vector<uint32_t> foundAddresses;

		SIZE_T patternSize = pattern.size();

		// Iterate through the memory pages
		while (address < (SIZE_T)sysInfo.lpMaximumApplicationAddress) {
			if (VirtualQueryEx(process, (LPCVOID)address, &mbi, sizeof(mbi))) {

				// Check if the memory region is readable
				if (mbi.State == MEM_COMMIT && (mbi.Protect & PAGE_READONLY || mbi.Protect & PAGE_READWRITE || mbi.Protect & PAGE_EXECUTE_READ)) {
					std::vector<uint8_t> buffer(mbi.RegionSize);
					SIZE_T bytesRead;

					// Read the memory region
					if (ReadProcessMemory(process, (LPCVOID)address, buffer.data(), mbi.RegionSize, &bytesRead)) {

						// Search for the pattern in the read memory
						for (SIZE_T i = 0; i < bytesRead - patternSize; i++) {
							if (memcmp(buffer.data() + i, pattern.data(), patternSize) == 0) {

								// Store the address of the found pattern
								foundAddresses.push_back(static_cast<uint32_t>(address + i));
							}
						}
					}
				}
				address += mbi.RegionSize; // Move to the next memory region
			}
			else {
				break; // Exit if VirtualQueryEx fails
			}
		}

		return foundAddresses; // Return the vector of found addresses
	}
};

