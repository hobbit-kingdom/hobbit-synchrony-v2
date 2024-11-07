#pragma once
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>

#include "Utilities.h"

const uint8_t BLANK_SPACE = 0;
class LogOption
{
public:

	LogOption()
	{
		isSupportsColor = LogUtilities::supportsColors();
	}

	

	void printLogOptions()
	{
		for (auto e : childOption)
		{
			if (isSupportsColor)
				std::cout << (e.isActive) ? LogUtilities::COLOR_TEXT::GREEN : LogUtilities::COLOR_TEXT::RED;
			else
				std::cout << (e.isActive) ? "V " : "X ";

			std::cout << e.optionName << std::endl;

			if (isSupportsColor)
				std::cout << LogUtilities::COLOR_TEXT::RESET;
		}
	}
	void printFullLogOptions(int recursion = 0)
	{
		for (int i = 0; i < childOption.size(); ++i)
		{
			std::cout << std::setw(recursion * BLANK_SPACE) << i << ") ";

			if (isSupportsColor)
				std::cout << (childOption[i].isActive) ? LogUtilities::COLOR_TEXT::GREEN : LogUtilities::COLOR_TEXT::RED;
			else
				std::cout << (childOption[i].isActive) ? "V " : "X ";

			std::cout << childOption[i].optionName << std::endl;

			if (isSupportsColor)
				std::cout << LogUtilities::COLOR_TEXT::RESET;

			if(childOption[i].childOption.size() > 0)
				printFullLogOptions(recursion + 1);
		}
	}
private:
	bool isSupportsColor = false;
	std::string optionName;
	std::vector<LogOption> childOption;
	bool isActive = false;
};