#ifndef CONFIG_HPP
# define CONFIG_HPP

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

class Config {
	public:
		Config();
		~Config();
};

std::vector<std::string> tokenize(const std::string& content);

#endif

