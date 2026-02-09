#include <iostream>
#include "Server.hpp"

int main(int argc, char **argv) {
	if (argc > 2) {
		std::cerr << "Usage: " << argv[0] << " [config_file]" << std::endl;
		return 1;
	}
	try{
		std::vector<std::string> tokens = tokenize("config/default.conf");
	} catch(std::exception &e){
		std::cout << e.what() << std::endl;
	}
	return 0;
}
