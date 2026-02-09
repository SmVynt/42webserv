#include "Config.hpp"

void printConfig(const std::vector<ServerConfig>& servers) {
	std::cout << "\n========== CONFIGURATION DEBUG ==========" << std::endl;
	std::cout << "Servers found: " << servers.size() << std::endl;

	for (size_t i = 0; i < servers.size(); ++i) {
		const auto& srv = servers[i];
		std::cout << "\n[SERVER " << i << "]" << std::endl;
		std::cout << "  Host:         " << srv.host << std::endl;
		std::cout << "  Port:         " << srv.port << std::endl;

		std::cout << "  Server Names: ";
		for (const auto& name : srv.server_names) std::cout << name << " ";
		std::cout << "\n  Max Body:     " << srv.client_max_body_size << " bytes" << std::endl;

		std::cout << "  Error Pages:  " << std::endl;
		for (const auto& [code, path] : srv.error_pages) {
			std::cout << "    " << code << " -> " << path << std::endl;
		}

		std::cout << "  Locations (" << srv.locations.size() << "):" << std::endl;
		for (const auto& loc : srv.locations) {
			std::cout << "    - Path: " << loc.path << std::endl;
			std::cout << "      Root:      " << loc.root << std::endl;
			std::cout << "      Index:     " << (loc.index.empty() ? "N/A" : loc.index) << std::endl;
			std::cout << "      Autoindex: " << (loc.autoindex ? "on" : "off") << std::endl;

			std::cout << "      Methods:   ";
			for (const auto& m : loc.methods) std::cout << m << " ";
			std::cout << std::endl;

			if (loc.upload_dir.has_value())
				std::cout << "      Upload:    " << *loc.upload_dir << std::endl;

			if (loc.cgi_path.has_value())
				std::cout << "      CGI Path:  " << *loc.cgi_path << std::endl;

			if (loc.cgi_ext.has_value())
				std::cout << "      CGI Ext:   " << *loc.cgi_ext << std::endl;
		}
	}
	std::cout << "========================================\n" << std::endl;
}

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