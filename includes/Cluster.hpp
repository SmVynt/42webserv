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
#include <algorithm>
#include "Request.hpp"
#include "Response.hpp"
#include "VirtualServer.hpp"
#include "utils.hpp"
#include "Logger.hpp"
#include "Methods.hpp"

// Default values (used when config doesn't specify them)
static const int			DEFAULT_CLIENT_TIMEOUT	= 60;
static const unsigned long	DEFAULT_MAX_BODY_SIZE	= 1048576; // 1MB
static const size_t			RECV_BUFFER_SIZE		= 4096;

enum FDType{
	FD_LISTENER,	// Listening socket: accepts new incoming connections
	FD_CLIENT,		// Client socket: handles HTTP requests and responses
	FD_CGI_IN,		// CGI input pipe: used to write POST body to the script's stdin
	FD_CGI_OUT,		// CGI output pipe: used to read the script's execution result
	FD_FILE			// Static file: used for non-blocking file I/O operations
};

enum ClientState{
	STATE_READING,
	STATE_PROCESSING,
	STATE_WRITING,
	STATE_DONE
};

struct FDMetadata{
	int			fd;				// The file descriptor number
	FDType		type;			// Purpose of this descriptor (from enum above)
	ClientState	client_state;	// State of the clients from enum above

	time_t		last_activity;	// Timestamp of the last I/O operation for timeout logic
	int			timeout_value;	// Timeout limit

	int			port;			// Port that client int using
	int			config_index;	// index in _config_data vector
	int			client_fd;		// Associated client socket (links CGI pipes to specific users)

	Request		request;		// Request obj
	Response	response;		// Response obj

	bool		is_ready_to_close;	// Flag to mark the descriptor for removal from the loop
};
class Cluster {
	public:
		Cluster();
		Cluster(const std::vector<ServerConfig>& config);
		Cluster(const Cluster& other) = delete;
		Cluster& operator=(const Cluster& other) = delete;
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
		void updatePollEvents(int fd, short events);
		bool handleClientResponse(int fd);

		// Utils for pollfds management and metadata
		void addFD(int fd, FDType type, int client_ref, int timeout);
		void removeFD(int fd);
		void updateActivity(int fd);
		void handleTimeout();
		void resetConnection(int fd);


		// Response
		Response generateErrorResponse(int code, int config_index);

		std::vector<VirtualServer>	_servers;
		std::vector<ServerConfig>	_config_data;
		std::vector<struct pollfd>	_pollfds;
		std::map<int, FDMetadata>	_fd_table;
		// Map for socket connection between configs or servers
		std::map<int, int>			_listen_sockets;
};
