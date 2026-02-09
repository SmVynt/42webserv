#include "Config.hpp"

Config::Config() {}

Config::~Config() {}

void printTokens(const std::vector<std::string>& tokens) {
	int i = 0;
	for (const auto& token : tokens) {
		std::cout << "Token[" << i << "]: \t" << token << std::endl;
		i++;
	}
}

std::string removeComments(const std::string &content) {
	std::string result;
	std::istringstream stream(content);
	std::string line;

	while (std::getline(stream, line)) {
		size_t commentPos = line.find('#');
		if (commentPos != std::string::npos) {
			line = line.substr(0, commentPos);
		}
		result += line + "\n";
	}
	return result;
}

std::string processFile(const std::string &filename){
	std::ifstream file(filename);
	if (!file.is_open()) throw std::runtime_error("Failed to open file");
	std::stringstream buffer;
	buffer << file.rdbuf();

	return removeComments(buffer.str());

}

std::vector<std::string> tokenize(const std::string &filename) {
	std::vector<std::string> tokens;
	std::string token;
	std::istringstream stream(processFile(filename));

	char c;
	while (stream.get(c)) {
		if (isspace(c)) continue;
		if (c == '{' || c == '}' || c == ';') {
			tokens.push_back(std::string(1, c));
		} else {
			token = c;
			while (stream.get(c) && !isspace(c) && c != '{' && c != '}' && c != ';') {
				token += c;
			}
			stream.putback(c);
			tokens.push_back(token);
		}
	}
	return tokens;
}