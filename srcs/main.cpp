#include <iostream>
#include "Server.hpp"
#include "Request.hpp"

int main(int argc, char **argv) {
	if (argc > 2) {
		std::cerr << "Usage: " << argv[0] << " [config_file]" << std::endl;
		return 1;
	}
	try{
		std::vector<std::string> tokens = (argc == 1) ? tokenize("config/default.conf") : tokenize(argv[1]);
		Config parser(tokens);
		std::vector<ServerConfig> servers = parser.parse();
		printConfig(servers);
		reqHardcode();
	} catch(std::exception &e){
		std::cout << e.what() << std::endl;
	}
	return 0;
}
