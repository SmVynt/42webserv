#pragma once
#include <poll.h>
#include <vector>
#include <string>
#include <map>
#include "Config.hpp"

class VirtualServer;

class Cluster {
	public:
		Cluster();
		Cluster(const std::vector<Config>& config);
		Cluster(const Cluster& other);
		Cluster& operator=(const Cluster& other);
		~Cluster();

		void	setupCluster();
		void	run();
	private:
		std::vector<VirtualServer>	_servers;
		std::vector<struct pollfd>	_pollfds;
		std::map<int, int>			_listen_sockets;
};
