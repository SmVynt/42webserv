#ifndef CONFIG_HPP
# define CONFIG_HPP

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <optional>
#include <map>

struct Location {
	std::string path;
	std::string root;
	std::vector<std::string> methods;
	std::string index;
	bool autoindex;
	std::optional<std::string> upload_dir;
	std::optional<std::string> cgi_path;
	std::optional<std::string> cgi_ext;
};

struct ServerConfig {
	int port;
	std::string host;
	std::vector<std::string> server_names;
	long client_max_body_size;
	std::map<int, std::string> error_pages;
	std::vector<Location> locations;
};

class Config {
	private:
		std::vector<std::string> _tokens;
		size_t _pos = 0;

		ServerConfig	parseServer();
		Location		parseLocation();

	public:
		Config(const std::vector<std::string> &tokens);
		~Config();

		std::vector<ServerConfig> parse();
};

std::vector<std::string>	tokenize(const std::string& content);
void						printConfig(const std::vector<ServerConfig>& servers);

#endif

