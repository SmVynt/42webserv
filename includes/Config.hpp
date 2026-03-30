#pragma once

#include "hub.hpp"

struct Location {
	std::string path;
	std::string root;
	std::vector<std::string> methods;
	std::string index;
	bool autoindex;
	std::optional<std::string> upload_dir;
	std::optional<std::string> cgi_path;
	std::optional<std::string> cgi_ext;
	std::pair<int, std::string> redirection;
	std::optional<unsigned long> client_max_body_size;
};

struct ServerConfig {
	int port;
	std::string host;
	std::vector<std::string> server_names;
	unsigned long client_max_body_size;
	std::map<int, std::string> error_pages;
	std::vector<Location> locations;
	int	client_timeout;
	int	session_timeout;
};

class Config {
	private:
		std::vector<std::string> _tokens;
		size_t _pos;

		/**
		 * @brief Parses a single `server` block from the token stream.
		 * @return Parsed server configuration.
		 */
		ServerConfig	parseServer();

		/**
		 * @brief Parses a single `location` block from the token stream.
		 * @return Parsed location configuration.
		 */
		Location		parseLocation();

		/**
		 * @brief Validates the final server configuration set.
		 * @param servers Parsed server list to validate.
		 */
		void			validate(const std::vector<ServerConfig> &servers);

	public:
		/**
		 * @brief Constructs a parser over already-tokenized configuration input.
		 * @param tokens Token sequence produced from a config file.
		 */
		Config(const std::vector<std::string> &tokens);

		/**
		 * @brief Destroys the configuration parser instance.
		 */
		~Config();

		/**
		 * @brief Parses all tokens into validated server configurations.
		 * @return List of parsed `ServerConfig` entries.
		 */
		std::vector<ServerConfig> parse();
};

/**
 * @brief Splits raw configuration text into parser tokens.
 * @param content Full text content of the configuration file.
 * @return Ordered list of lexical tokens.
 */
std::vector<std::string>	tokenize(const std::string &content);

/**
 * @brief Prints parsed configuration for debugging/inspection.
 * @param servers Parsed server configuration list to print.
 */
void						printConfig(const std::vector<ServerConfig> &servers);
