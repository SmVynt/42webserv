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
#include "Request.hpp"
#include "VirtualServer.hpp"
#include <algorithm>

enum FDType{
	FD_LISTENER,	// Listening socket: accepts new incoming connections
	FD_CLIENT,		// Client socket: handles HTTP requests and responses
	FD_CGI_IN,		// CGI input pipe: used to write POST body to the script's stdin
	FD_CGI_OUT,		// CGI output pipe: used to read the script's execution result
	FD_FILE			// Static file: used for non-blocking file I/O operations
};

struct FDMetadata{
	int			fd;				// The file descriptor number
	FDType		type;			// Purpose of this descriptor (from enum above)
	time_t		last_activity;	// Timestamp of the last I/O operation for timeout logic
	int			client_fd;		// Associated client socket (links CGI pipes to specific users)
	std::string	write_buffer;	// Buffer for data waiting to be sent when POLLOUT is ready
	bool		is_ready_to_close;	// Flag to mark the descriptor for removal from the loop

	// Constructor: initializes the structure with safe default values
	FDMetadata() :	fd(-1),
					type(FD_LISTENER),
					last_activity(time(NULL)),
					client_fd(-1),
					is_ready_to_close(false) {}
};
class Cluster {
	public:
		Cluster();
		Cluster(const std::vector<ServerConfig>& config);
		Cluster(const Cluster& other);
		Cluster& operator=(const Cluster& other);
		~Cluster();
		// Method that setups initial settings for sockets
		void	setupCluster();
		// Main loop with poll()
		void	run();
	private:
		// Utils for run()
		void acceptNewConnection(int listen_fd);
		bool handleClientRequest(int fd);
		void closeConnection(int fd);

		// Utils for pollfds management and metadata
		void addFD(int fd, FDType type, int client_ref = -1);
		void removeFD(int fd);
		void updateActivity(int fd);
		void handleTimeout();

		std::vector<VirtualServer>	_servers;
		std::vector<ServerConfig>	_config_data;
		std::vector<struct pollfd>	_pollfds;
		std::map<int, FDMetadata>	_fd_table;
		// Map for socket connection between configs or servers
		std::map<int, int>			_listen_sockets;
		// Map to store request status of each client
		std::map<int, Request>		_requests;
};
