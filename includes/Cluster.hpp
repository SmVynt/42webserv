#pragma once
#include <poll.h>
#include <vector>
#include <string>
#include <map>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <cstring>
#include "Config.hpp"

class VirtualServer;

class Cluster {
	public:
		Cluster();
		Cluster(const std::vector<ServerConfig>& config);
		Cluster(const Cluster& other);
		Cluster& operator=(const Cluster& other);
		~Cluster();

		void	setupCluster();
		void	run();
	private:
		std::vector<VirtualServer>	_servers;
		std::vector<ServerConfig>	_config_data;
		std::vector<struct pollfd>	_pollfds;
		std::map<int, int>			_listen_sockets;
};
