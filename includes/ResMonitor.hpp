#pragma once

#include "hub.hpp"

# include "Logger.hpp"

class ResMonitor {
	private:
		static float	_FDUsageThreshold;
	public:
		static size_t	getMaxFDAmount();
		static size_t	getOpenFDAmount();
};
