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
#include <poll.h>
#include "Config.hpp"
#include "Request.hpp"
#include "VirtualServer.hpp"

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
		void acceptNewConnection(int listen_fd);
		bool handleClientRequest(size_t pollfd_index);
		void closeConnection(size_t pollfd_index);

		std::vector<VirtualServer>	_servers;
		std::vector<ServerConfig>	_config_data;
		std::vector<struct pollfd>	_pollfds;
		std::map<int, int>			_listen_sockets;
		std::map<int, Request>		_requests;
};
