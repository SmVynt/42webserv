#include "utils.hpp"
#include <errno.h>

std::string	loadFile(const std::string &path){
	std::ifstream file(path.c_str());
	if (!file.is_open())
		return "";
	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

void	safeClose(int &fd) {
	if (fd >= 0)
	{
		if (close(fd) == -1) {
			// EBADF is expected if fd was already closed elsewhere
			if (errno != EBADF) {
				Logger::error("Error: close() failed: " + std::string(strerror(errno)));
			}
		}
		fd = -1;
	}
}
