#pragma once

#include "hub.hpp"

enum LogLevel {
	DEBUG,
	INFO,
	WARNING,
	ERROR
};

class Logger {
	private:
		static LogLevel		_minLevel;
		static LogLevel		_maxLevel;
		static bool			_useTimestamps;
		static std::string	getTimestamp();
		static const char *	getLevelString(LogLevel level);
		static const char *	getLevelColor(LogLevel level);

	public:
		static void	setLogLevel(LogLevel level);
		static void	enableTimestamps(bool enable);

		static void	debug(const std::string &message);
		static void	info(const std::string &message);
		static void	warning(const std::string &message);
		static void	error(const std::string &message);

		static int	log(LogLevel level, const std::string &message);
};
