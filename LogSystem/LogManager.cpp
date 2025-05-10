#include "LogManager.h"
using Ptr = std::shared_ptr<LogOption>;

LogOption::LogOption(const std::string& name, LogLevel level)
	: name_(name), level_(level), active_(true) {}

void LogOption::AddChild(const Ptr& child) {
	std::lock_guard<std::mutex> lock(mutex_);
	child->parent_ = shared_from_this();
	children_.push_back(child);
}

void LogOption::RemoveChild(const Ptr& child) {
	std::lock_guard<std::mutex> lock(mutex_);
	auto it = std::find(children_.begin(), children_.end(), child);
	if (it != children_.end()) {
		(*it)->parent_.reset();
		children_.erase(it);
	}
}

void LogOption::ToggleStatus() {
	std::lock_guard<std::mutex> lock(mutex_);
	active_ = !active_;
}

void LogOption::SetStatus(bool active) {
	std::lock_guard<std::mutex> lock(mutex_);
	active_ = active;
}

bool LogOption::IsActive() const {
	std::lock_guard<std::mutex> lock(mutex_);
	return active_;
}

std::string LogOption::GetName() const {
	return name_;
}

LogLevel LogOption::GetLevel() const {
	return level_;
}

void LogOption::SetLevel(LogLevel level) {
	level_ = level;
}

Ptr LogOption::GetParent() const {
	std::lock_guard<std::mutex> lock(mutex_);
	return parent_.lock();
}

std::vector<Ptr> LogOption::GetChildren() const {
	std::lock_guard<std::mutex> lock(mutex_);
	return children_;
}

std::mutex LogOption::consoleMutex_;

void LogOption::logDisplayMessage(LogLevel msgLevel, const std::ostringstream& oss)
{
	if (msgLevel < LogManager::Instance().GetGlobalLogLevel())
		return; // Return empty stream if log level is too low

	if (!IsActive())
		return; // Return empty stream if not active

	std::string message = "";

	//if (msgLevel != LogLevel::Log_Prompt)
	{
		message = GetLevelPrefix(msgLevel);
		if (name_ != "")
		{
			message += "[" + name_ + "] ";
		}
	}
	message += std::string(4 * depth, ' ');
	// Lock the console mutex to prevent overlapping output
	std::lock_guard<std::mutex> lock(consoleMutex_);

	std::cout << message << oss.str() << std::endl;
}

std::string LogOption::GetLevelPrefix(LogLevel level) {
	std::ostringstream oss;

	// Set the width for the log level prefix
	oss << std::left << std::setw(10); // Adjust the width as needed

	switch (level) {
	case LogLevel::Log_Debug: oss << "[DEBUG] "; break;
	case LogLevel::Log_Info: oss << "[INFO] "; break;
	case LogLevel::Log_Warning: oss << "[WARNING] "; break;
	case LogLevel::Log_Error: oss << "[ERROR] "; break;
	case LogLevel::Log_Prompt: oss << "[PROMPT] "; break;
	default: return "";
	}

	return oss.str();
}


LogManager& LogManager::Instance() {
	static LogManager instance;
	return instance;
}