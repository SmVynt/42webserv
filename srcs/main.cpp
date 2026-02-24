#include <iostream>
#include <csignal>
#include "Request.hpp"
#include "VirtualServer.hpp"
#include "Cluster.hpp"

// Global for signal handling
Cluster* g_cluster_instance = nullptr;

// Signal handler for graceful shutdown
void signal_handler(int sig) {
	if (sig == SIGTERM || sig == SIGINT) {
		if (g_cluster_instance) {
			g_cluster_instance->requestShutdown();
		}
	}
}

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
		g_cluster_instance = &webserv;

		// Register signal handlers for graceful shutdown
		std::signal(SIGTERM, signal_handler);
		std::signal(SIGINT, signal_handler);

		webserv.setupCluster();

		webserv.run();

		// Cleanup
		g_cluster_instance = nullptr;

	} catch(std::exception &e){
		std::cout << e.what() << std::endl;
	}
	return 0;
}
