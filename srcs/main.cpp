#include <iostream>
#include <csignal>
#include "Request.hpp"
#include "VirtualServer.hpp"
#include "Cluster.hpp"

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
		// reqHardcode();
		// reqChunkedHardcode();

		Cluster webserv(servers);
		cluster_reference() = &webserv;

		// Register signal handlers for graceful shutdown
		struct sigaction sa{};
		sa.sa_handler = signal_handler;
		sigemptyset(&sa.sa_mask);
		sigaction(SIGTERM, &sa, NULL);
		sigaction(SIGINT, &sa, NULL);

		webserv.setupCluster();

		webserv.run();

		cluster_reference() = nullptr;

	} catch(std::exception &e){
		std::cout << e.what() << std::endl;
	}
	return 0;
}
