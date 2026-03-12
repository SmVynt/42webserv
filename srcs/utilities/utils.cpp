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
			// Get the two hex digits
			std::string hex = str.substr(i + 1, 2);
			std::istringstream iss(hex);
			int value;
			if (iss >> std::hex >> value) {
				result += static_cast<char>(value);
				i += 2; // Skip the next 2 characters
			} else {
				result += str[i]; // Invalid hex, keep the %
			}
		} else if (str[i] == '+') {
			result += ' '; // Convert + to space (query string convention)
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
