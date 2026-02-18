#ifndef RESMONITOR_HPP
# define RESMONITOR_HPP

# include <sys/resource.h>
# include <cstring>
# include "Logger.hpp"

class ResMonitor {
	private:
		static float	_FDUsageThreshold;;
	public:
		static size_t	getMaxFDAmount();
		static size_t	getOpenFDAmount();
		static size_t	getAvailableFDAmount();
		static bool		isApproachingLimit();
};

#endif
