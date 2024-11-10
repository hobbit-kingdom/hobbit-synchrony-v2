#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <algorithm>
#include <sstream>
#include <iomanip>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

class LogManager;

enum class LogLevel {
    Log_Debug,
    Log_Info,
    Log_Warning,
    Log_Error,
    Log_Prompt,
    Log_None
};

class LogOption : public std::enable_shared_from_this<LogOption> {
public:
    using Ptr = std::shared_ptr<LogOption>;
    LogOption() {};
    LogOption(const std::string& name, LogLevel level = LogLevel::Log_Info);

    void AddChild(const Ptr& child);

    void RemoveChild(const Ptr& child);

    void ToggleStatus();

    void SetStatus(bool active);

    bool IsActive() const;

    std::string GetName() const;

    LogLevel GetLevel() const;

    void SetLevel(LogLevel level);

    Ptr GetParent() const;

    std::vector<Ptr> GetChildren() const;

    // Function to convert any type to string
    template <typename T>
    std::string toString(const T& value) {
        std::ostringstream oss;
        oss << value; // This will work for most types that support the << operator
        return oss.str();
    }

    // Variadic template function to handle multiple arguments
    template <typename... Args>
    void LogMessage(LogLevel msgLevel, const Args&... args) {
        std::ostringstream combinedStream;

        // Fold expression to combine all arguments into a single string
        ((combinedStream << toString(args) << " "), ...); // Add a space after each argument

        std::string combinedString = combinedStream.str();

        // Trim the trailing space
        if (!combinedString.empty()) {
            combinedString.pop_back();
        }

        logDisplayMessage(msgLevel, combinedStream);
    }


    void logDisplayMessage(LogLevel msgLevel, const std::ostringstream& oss);


private:
    std::string name_;
    LogLevel level_;
    bool active_;
    mutable std::mutex mutex_;
    static std::mutex consoleMutex_; // Static mutex for console output synchronization
    std::weak_ptr<LogOption> parent_;
    std::vector<Ptr> children_;

    std::string GetLevelPrefix(LogLevel level);
};

class LogManager {
public:
    static LogManager& Instance();

    LogOption::Ptr CreateLogOption(const std::string& name, LogLevel level = LogLevel::Log_Info) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto logOption = std::make_shared<LogOption>(name, level);
        rootLogs_.push_back(logOption);
        return logOption;
    }

    void AddLogOption(const LogOption::Ptr& logOption, const LogOption::Ptr& parent = nullptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (parent) {
            parent->AddChild(logOption);
        }
        else {
            rootLogs_.push_back(logOption);
        }
    }

    void MoveLogOption(const LogOption::Ptr& logOption, const LogOption::Ptr& newParent) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (auto parent = logOption->GetParent()) {
            parent->RemoveChild(logOption);
        }
        else {
            auto it = std::find(rootLogs_.begin(), rootLogs_.end(), logOption);
            if (it != rootLogs_.end()) {
                rootLogs_.erase(it);
            }
        }

        if (newParent) {
            newParent->AddChild(logOption);
        }
        else {
            rootLogs_.push_back(logOption);
            logOption->SetStatus(true);
        }
    }
    // Overloaded MoveLogOption function
    void MoveLogOption(const std::string& logOptionName, const std::string& newParentName) {

        // Find the log option by name
        LogOption::Ptr logOption = FindLogOptionByName(rootLogs_, logOptionName);
        LogOption::Ptr newParent = FindLogOptionByName(rootLogs_, newParentName);

        if (!logOption) {
            std::cerr << "Error: Log option '" << logOptionName << "' not found." << std::endl;
            return;
        }

        // Move the log option to the new parent
        MoveLogOption(logOption, newParent);
    }
    void DisplayHierarchy(bool useColor = true) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& log : rootLogs_) {
            DisplayLogOption(log, 0, useColor);
        }
    }

    LogLevel GetGlobalLogLevel() const {
        return globalLogLevel_;
    }

    void SetGlobalLogLevel(LogLevel level) {
        globalLogLevel_ = level;
    }

private:
    LogManager() : globalLogLevel_(LogLevel::Log_Debug) {}

    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;

    std::vector<LogOption::Ptr> rootLogs_;
    mutable std::mutex mutex_;
    LogLevel globalLogLevel_;

    void DisplayLogOption(const LogOption::Ptr& logOption, int indent, bool useColor) {
        std::string indentation(indent * 2, ' ');

        if (useColor) {
            SetConsoleColor(logOption->IsActive());
        }

        std::cout << indentation << (logOption->IsActive() ? "[Active] " : "[Inactive] ") << logOption->GetName() << std::endl;

        if (useColor) {
            ResetConsoleColor();
        }

        for (const auto& child : logOption->GetChildren()) {
            DisplayLogOption(child, indent + 1, useColor);
        }
    }

    void SetConsoleColor(bool isActive) {
#ifdef _WIN32
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        if (isActive) {
            SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        }
        else {
            SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
        }
#else
        if (isActive) {
            std::cout << "\033[32m";
        }
        else {
            std::cout << "\033[31m";
        }
#endif
    }

    void ResetConsoleColor() {
#ifdef _WIN32
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#else
        std::cout << "\033[0m";
#endif
    }

    LogOption::Ptr FindLogOptionByName(const std::vector<LogOption::Ptr>& logOptions, const std::string& name) {
        for (const auto& logOption : logOptions) {
            // Check if the current log option matches the name
            if (logOption->GetName() == name) {
                return logOption; // Return the found log option
            }

            // Recursively search in the children
            LogOption::Ptr foundInChildren = FindLogOptionByName(logOption->GetChildren(), name);
            if (foundInChildren) {
                return foundInChildren; // Return if found in children
            }
        }
        return nullptr; // Return nullptr if not found
    }
};

