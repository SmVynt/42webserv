#include <iostream>
#include <signal.h>
#include "Request.hpp"
#include "Cluster.hpp"

int main(int argc, char **argv) {
	srand (time(NULL));
	Logger::setLogLevel(LogLevel::INFO);
	if (argc > 2) {
		std::cerr << "Usage: " << argv[0] << " [config_file]" << std::endl;
		return 1;
	}
	try{
		std::vector<std::string> tokens = (argc == 1) ? tokenize("config/default.conf") : tokenize(argv[1]);
		Config parser(tokens);
		std::vector<ServerConfig> servers = parser.parse();
		printConfig(servers);

		Cluster webserv(servers);
		cluster_reference() = &webserv;

		signal(SIGTERM, signal_handler);
		signal(SIGINT, signal_handler);
		signal(SIGPIPE, SIG_IGN);

		webserv.setupCluster();

		webserv.run();

		cluster_reference() = nullptr;

	} catch(std::exception &e){
		Logger::error(e.what());
	}
	return 0;
}
