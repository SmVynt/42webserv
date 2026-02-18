#include "Logger.hpp"

// Presetting default log level and timestamp usage
LogLevel	Logger::_minLevel = INFO;
LogLevel	Logger::_maxLevel = ERROR;
bool		Logger::_useTimestamps = true;

std::string	Logger::getTimestamp(){
	std::time_t now = std::time(nullptr);
	char buf[20];
	std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
	return std::string(buf);
};

const char*	Logger::getLevelString(LogLevel level){
	switch (level) {
		case DEBUG: return "DEBUG";
		case INFO: return "INFO";
		case WARNING: return "WARNING";
		case ERROR: return "ERROR";
		default: return "UNKNOWN";
	}
};

const char*	Logger::getLevelColor(LogLevel level){
	switch (level) {
		case DEBUG: return "\033[36m"; // Cyan
		case INFO: return "\033[32m"; // Green
		case WARNING: return "\033[33m"; // Yellow
		case ERROR: return "\033[31m"; // Red
		default: return "\033[0m"; // Reset
	}
};

void	Logger::setLogLevel(LogLevel level){
	_minLevel = level;
};

void	Logger::enableTimestamps(bool enable){
	_useTimestamps = enable;
};

void	Logger::debug(const std::string &message) {Logger::log(DEBUG, message);};
void	Logger::info(const std::string &message) {Logger::log(INFO, message);};
void	Logger::warning(const std::string &message) {Logger::log(WARNING, message);};
void	Logger::error(const std::string &message) {Logger::log(ERROR, message);};

int	Logger::log(LogLevel level, const std::string &message)
{
	if (level < _minLevel || level > _maxLevel)
		return -1;

	std::string header = getLevelColor(level);
	if (_useTimestamps) {
		header += "[" + getTimestamp() + "] ";
	}
	header += "[" + std::string(getLevelString(level)) + "] ";
	if (level == ERROR)
		std::cerr << header << message << "\033[0m" << std::endl;
	else
		std::cout << header << message << "\033[0m" << std::endl;

	return static_cast<int>(level);
};
