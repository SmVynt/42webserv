#include "utils.hpp"
#include <errno.h>
#include <sstream>
#include <iomanip>

std::string	loadFile(const std::string &path){
	std::ifstream file(path.c_str());
	if (!file.is_open())
		return "";
	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

std::string	urlDecode(const std::string &str) {
	std::string result;
	result.reserve(str.length());

	for (size_t i = 0; i < str.length(); ++i) {
		if (str[i] == '%' && i + 2 < str.length()) {
			std::string hex = str.substr(i + 1, 2);
			std::istringstream iss(hex);
			int value;
			if (iss >> std::hex >> value && hex != "2F" && hex != "2f") {
				result += static_cast<char>(value);
				i += 2;
				Logger::debug("Decoded '%" + hex + "' to '" + std::string(1, static_cast<char>(value)) + "'");
			} else {
				result += str[i];
			}
		} else if (str[i] == '+') {
			result += ' ';
		} else {
			result += str[i];
		}
	}

	return result;
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
