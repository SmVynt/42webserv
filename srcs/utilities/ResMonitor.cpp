#include "ResMonitor.hpp"
#include <dirent.h>

// Presetting default Monitor threshold to 90% usage
float	ResMonitor::_FDUsageThreshold = 0.9;

size_t	ResMonitor::getMaxFDAmount() {
	struct rlimit rl;
	if (getrlimit(RLIMIT_NOFILE, &rl) == -1) {
		Logger::error("Error: getrlimit() failed");
		return 1024;
	}
	return rl.rlim_cur;
}

size_t	ResMonitor::getOpenFDAmount() {
	DIR* dp = opendir("/proc/self/fd");
	if (dp == NULL) {
		Logger::error("Error: opendir() failed on /proc/self/fd");
		return -1;
	}

	int count = 0;
	struct dirent* de;
	while ((de = readdir(dp)) != NULL) {
		// Skip "." (current directory) and ".." (parent directory) entries
		if (de->d_name[0] != '.') {
			count++;
		}
	}

	closedir(dp);
	if (count <= 3)
		return 0; // Ignore standard input, output, and error
	return (count - 3);
}

size_t	ResMonitor::getAvailableFDAmount() {
	size_t max_fd = getMaxFDAmount();
	size_t open_fd = getOpenFDAmount();
	if (open_fd >= max_fd)
		return 0;
	return max_fd - open_fd;
}

bool ResMonitor::isApproachingLimit() {
	size_t max_fd = getMaxFDAmount();
	size_t open_fd = getOpenFDAmount();
	if (max_fd == 0)
		return false;
	float usage = static_cast<float>(open_fd) / static_cast<float>(max_fd);
	if (usage >= _FDUsageThreshold) {
		Logger::warning("FD usage approaching limit: " + std::to_string(usage * 100) + "% used");
		return true;
	}
	return false;
}
