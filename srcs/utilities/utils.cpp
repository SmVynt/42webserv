#include "utils.hpp"

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
			// std::cerr << "Error: close() failed" << std::endl;
			Logger::error("Error: close() failed");
		}
		fd = -1;
	}
}
