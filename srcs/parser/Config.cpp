#include "Config.hpp"

Config::Config(const std::vector<std::string> &tokens): _tokens(tokens), _pos(0) {}

Config::Config(const Config &other): _tokens(other._tokens), _pos(other._pos) {}

Config &Config::operator=(const Config &other){
	if (this != &other){
		_tokens = other._tokens;
		_pos = other._pos;
	}
	return *this;
}

Config::~Config() {}

std::vector<ServerConfig> Config::parse(){
	std::vector<ServerConfig> servers;
	while(_pos < _tokens.size()){
		if (_tokens[_pos] == "server")
			servers.push_back(parseServer());
		else
			throw std::runtime_error("Unknown global token: " + _tokens[_pos]);
	}
	// validate(servers);
	return servers;
}

ServerConfig Config::parseServer() {
	ServerConfig config;
	config.port = 0;
	config.client_max_body_size = 1024 * 1024;
	config.client_timeout = 60;
	config.session_timeout = 300;
	if (_pos >= _tokens.size() || _tokens[_pos++] != "server")
		throw std::runtime_error("Parser error: expected 'server'");

	if (_pos >= _tokens.size() || _tokens[_pos++] != "{")
		throw std::runtime_error("Parser error: expected '{' after server");

	while (_pos < _tokens.size() && _tokens[_pos] != "}") {
		std::string key = _tokens[_pos++];

		if (key == "listen") {
			if (_pos >= _tokens.size())
				throw std::runtime_error("Unexpected EOF after listen");
			config.port = std::stoi(_tokens[_pos++]);
		}
		else if (key == "server_name") {
			if (_pos >= _tokens.size())
				throw std::runtime_error("Unexpected EOF after server_name");
			config.server_names.push_back(_tokens[_pos++]);
		}
		else if (key == "host") {
			if (_pos >= _tokens.size())
				throw std::runtime_error("Unexpected EOF after host");
			config.host = _tokens[_pos++];
		}
		else if (key == "error_page") {
			if (_pos + 1 >= _tokens.size())
				throw std::runtime_error("Missing arguments for error_page");
			int code = std::stoi(_tokens[_pos++]);
			config.error_pages[code] = _tokens[_pos++];
		}
		else if (key == "client_timeout"){
			if (_pos >= _tokens.size())
				throw std::runtime_error("Unexpected EOF after client_timeout");
			config.client_timeout = std::stoi(_tokens[_pos++]);
		}
		else if (key == "session_timeout"){
			if (_pos >= _tokens.size())
				throw std::runtime_error("Unexpected EOF after session_timeout");
			config.session_timeout = std::stoi(_tokens[_pos++]);
		}
		else if (key == "client_max_body_size") {
			if (_pos >= _tokens.size())
				throw std::runtime_error("Missing value for client_max_body_size");
			size_t p;
			unsigned long bytes = std::stol(_tokens[_pos], &p);
			int mult = 1;
			if (p < _tokens[_pos].length()) {
				char unit = _tokens[_pos][p];
				if (unit == 'M' || unit == 'm')
					mult = 1048576;
				else if (unit == 'G' || unit == 'g')
					mult = 1073741824;
				else if (unit == 'K' || unit == 'k')
					mult = 1024;
				else if (unit != ';')
					throw std::runtime_error("Invalid unit for client_max_body_size");
				if (mult > 1 && bytes > (std::numeric_limits<unsigned long>::max() / mult))
					throw std::runtime_error("client_max_body_size is too large");

				bytes *= mult;
			}
			config.client_max_body_size = bytes;
			_pos++;
		}
		else if (key == "location") {
			config.locations.push_back(parseLocation());
			continue;
		}
		else {
			throw std::runtime_error("Unknown server directive: " + key);
		}

		if (_pos >= _tokens.size() || _tokens[_pos++] != ";")
			throw std::runtime_error("Expected ';' after " + key);
	}

	if (_pos >= _tokens.size() || _tokens[_pos++] != "}")
		throw std::runtime_error("Expected '}' at the end of server block");

	return config;
}

Location Config::parseLocation() {
	Location loc;
	loc.autoindex = false;
	loc.index = "index.html";

	if (_pos >= _tokens.size())
		throw std::runtime_error("Unexpected EOF in location");
	loc.path = _tokens[_pos++];

	if (_pos >= _tokens.size() || _tokens[_pos++] != "{")
		throw std::runtime_error("Expected '{' after location path");

	while (_pos < _tokens.size() && _tokens[_pos] != "}") {
		std::string key = _tokens[_pos++];

		if (key == "root") {
			if (_pos >= _tokens.size())
				throw std::runtime_error("Missing root value");
			loc.root = _tokens[_pos++];
		}
		else if (key == "index") {
			if (_pos >= _tokens.size())
				throw std::runtime_error("Missing index value");
			loc.index = _tokens[_pos++];
		}
		else if (key == "methods") {
			while (_pos < _tokens.size() && _tokens[_pos] != ";") {
				loc.methods.push_back(_tokens[_pos++]);
			}
			if (loc.methods.empty())
				throw std::runtime_error("No methods specified");
		}
		else if (key == "autoindex") {
			if (_pos >= _tokens.size())
				throw std::runtime_error("Missing autoindex value");
			loc.autoindex = (_tokens[_pos++] == "on");
		}
		else if (key == "upload_dir") {
			if (_pos >= _tokens.size())
				throw std::runtime_error("Missing upload_dir value");
			loc.upload_dir = _tokens[_pos++];
		}
		else if (key == "cgi_path") {
			if (_pos >= _tokens.size())
				throw std::runtime_error("Missing cgi_path value");
			loc.cgi_path = _tokens[_pos++];
		}
		else if (key == "cgi_extension") {
			if (_pos >= _tokens.size())
				throw std::runtime_error("Missing cgi_ext value");
			loc.cgi_ext = _tokens[_pos++];
		}
		else if (key == "return") {
			if (_pos >= _tokens.size())
				throw std::runtime_error("Missing return value");
			int code = std::stoi(_tokens[_pos++]);
			std::string url = _tokens[_pos++];
			loc.redirection = {code, url};
		}
		else if (key == "client_max_body_size") {
			if (_pos >= _tokens.size())
				throw std::runtime_error("Missing value for client_max_body_size");
			size_t p;
			unsigned long bytes = std::stol(_tokens[_pos], &p);
			int mult = 1;
			if (p < _tokens[_pos].length()) {
				char unit = _tokens[_pos][p];
				if (unit == 'M' || unit == 'm')
					mult = 1048576;
				else if (unit == 'G' || unit == 'g')
					mult = 1073741824;
				else if (unit == 'K' || unit == 'k')
					mult = 1024;
				else if (unit != ';')
					throw std::runtime_error("Invalid unit for client_max_body_size");
				if (mult > 1 && bytes > (std::numeric_limits<unsigned long>::max() / mult))
					throw std::runtime_error("client_max_body_size is too large");

				bytes *= mult;
			}
			loc.client_max_body_size = bytes;
			_pos++;
		}
		else {
			throw std::runtime_error("Unknown location directive: " + key);
		}

		if (_pos >= _tokens.size() || _tokens[_pos++] != ";")
			throw std::runtime_error("Expected ';' after " + key);
	}

	if (_pos >= _tokens.size() || _tokens[_pos++] != "}")
		throw std::runtime_error("Expected '}' at the end of location block");

	return loc;
}

void	Config::validate(const std::vector<ServerConfig> &servers) {
	if (servers.empty())
		throw std::runtime_error("No servers defined in config");

	std::set<std::pair<std::string, int>> unique_servers;

	for (const ServerConfig &srv : servers) {
		if (srv.port <= 0 || srv.port > 65535)
			throw std::runtime_error("Invalid port: " + std::to_string(srv.port));

		// if (!unique_servers.insert({srv.host, srv.port}).second)
		// 	throw std::runtime_error("Duplicate server on " + srv.host + ":" + std::to_string(srv.port));

		for (const Location &loc : srv.locations) {
			if (loc.path.empty())
				throw std::runtime_error("Location path cannot be empty");

			if (!loc.root.empty() && !std::filesystem::exists(loc.root)) {
				std::cerr << "Warning: root path '" << loc.root << "' does not exist." << std::endl;
			}

			for (const std::string &method : loc.methods) {
				if (method != "GET" && method != "POST" && method != "DELETE" && method != "HEAD")
					throw std::runtime_error("Unsupported method: " + method);
			}

			if (loc.cgi_path.has_value()) {
				if (!std::filesystem::exists(*loc.cgi_path))
					throw std::runtime_error("CGI interpreter not found: " + *loc.cgi_path);
			}
		}
	}
	std::cout << "Config validation: OK!" << std::endl;
}
