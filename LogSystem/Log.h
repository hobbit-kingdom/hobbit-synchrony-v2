#pragma once
#include <iostream>
#include <sstream>
#include <string>
#include "Utilities.h"
// Define log levels
enum LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    NONE
};



class Logger
{
    std::string header = "GENERAL";

    template<typename... Args>
    void log(LogLevel level, COLOR_TEXT color = COLOR_TEXT.RESET, Args... args) {

        std::ostringstream oss;

        switch (level) {
        case DEBUG: oss << "[DEBUG] "; break;
        case INFO: oss << "[INFO] "; break;
        case WARNING: oss << "[WARNING] "; break;
        case ERROR: oss << "[ERROR] "; break;
        }

        oss << "[" << header << "] ";
        (oss << ... << args);

        std::cout << color << oss.str() << RESET << std::endl;
    }
};