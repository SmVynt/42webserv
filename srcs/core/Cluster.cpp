#include "Cluster.hpp"

Cluster::Cluster(){}
Cluster::Cluster(const std::vector<ServerConfig>& config) : _config_data(config) {}
Cluster::Cluster(const Cluster& other)
{
	*this = other;
}
Cluster& Cluster::operator=(const Cluster& other)
{
	if (this != &other)
	{
		_config_data = other._config_data;
		_listen_sockets = other._listen_sockets;
		_pollfds = other._pollfds;
		_servers = other._servers;
	}
	return *this;
}
Cluster::~Cluster()
{
	for (const auto& [fd, port] : _listen_sockets)
	{
		if (fd >= 0)
			close(fd);
	}
}
void	Cluster::setupCluster()
{
	for (const auto& pfd : _pollfds)
	{
		if (pfd.fd >= 0)
			close(pfd.fd);
	}
	_pollfds.clear();
	_listen_sockets.clear();

	int error_code = 0;
	for (const auto& config : _config_data)
	{
		int port = config.port;
		bool already_open = false;
		for (const auto& [fd, open_port] : _listen_sockets)
		{
			if (port == open_port)
			{
				already_open = true;
				break;
			}
		}
		if (already_open == true)
			continue;
		int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (socket_fd < 0)
		{
			error_code = errno;
			throw std::runtime_error("Socket creation failed: " + std::string(strerror(error_code)));
		}
		int option = 1;
		if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0)
		{
			error_code = errno;
			close(socket_fd);
			throw std::runtime_error("setsockopt(SO_REUSEADDR) failed: " + std::string(strerror(error_code)));
		}
		if (fcntl(socket_fd, F_SETFL, O_NONBLOCK) < 0)
		{
			error_code = errno;
			close(socket_fd);
			throw std::runtime_error("fcntl(O_NONBLOCK) failed: " + std::string(strerror(error_code)));
		}
		sockaddr_in address{};
		address.sin_family = AF_INET;
		address.sin_port = htons(config.port);
		address.sin_addr.s_addr = INADDR_ANY;
		if (bind(socket_fd, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) < 0)
		{
			error_code = errno;
			close(socket_fd);
			throw std::runtime_error("bind failed on port: " + std::to_string(config.port) + ": " + std::string(strerror(error_code)));
		}
		if (listen(socket_fd, 128) < 0)
		{
			error_code = errno;
			close(socket_fd);
			throw std::runtime_error("listen failed: " + std::string(strerror(error_code)));
		}

		_listen_sockets[socket_fd] = config.port;

		_pollfds.push_back({socket_fd, POLLIN, 0});

		std::cout << "Listening on port " << port << " (fd: " << socket_fd << ")" << std::endl;
	}
}
void	Cluster::run()
{
	std::cout << "--- Server is starting the event loop ---" << std::endl;

	while (true){
		int ret = poll(_pollfds.data(), _pollfds.size(), -1);
		if (ret < 0){
			if (errno == EINTR)
				continue;
			throw std::runtime_error("Poll filed: " + std::string(strerror(errno)));
		}
		for (size_t i = 0; i < _pollfds.size(); ++i){
			if (_pollfds[i].revents == 0)
				continue;
			if (_pollfds[i].revents & (POLLHUP | POLLERR | POLLNVAL)){
				closeConnection(i);
				--i;
				continue;
			}
			if (_pollfds[i].revents & POLLIN){
				if (_listen_sockets.count(_pollfds[i].fd)){
					acceptNewConnection(_pollfds[i].fd);
				} else {
					if (handleClientRequest(i) == true)
						--i;
				}
			}
		}
	}
}

void Cluster::acceptNewConnection(int listen_fd)
{
	struct sockaddr_in client_addr{};
	socklen_t client_len = sizeof(client_addr);

	int client_fd = accept(listen_fd, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);

	if (client_fd < 0){
		std::cerr << "Warning: accept() failed: " << strerror(errno) << std::endl;
		return;
	}
	if (fcntl(client_fd, F_SETFL, O_NONBLOCK) < 0){
		std::cerr << "Warning: fcntl() failed for FD " << client_fd << ": " << strerror(errno) << std::endl;
		close(client_fd);
		return;
	}
	_pollfds.push_back({client_fd, POLLIN, 0});
	std::cout << "New client connected: FD " << client_fd << std::endl;
}


bool Cluster::handleClientRequest(size_t pollfd_index)
{
	int client_fd = _pollfds[pollfd_index].fd;
	char buffer[4096];

	int byte_reads = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
	if (byte_reads > 0){
		buffer[byte_reads] = '\0';
		std::cout << "--- RAW REQUEST FROM CLIENT ---\n" << buffer << "\n-------------------------------" << std::endl;
		// TO DO: Sasha must create request class that will revieve and handle data from this buffer
		std::string dummyResponse = "HTTP/1.1 200 OK\r\nContent-Length: 19\r\n\r\nHello from Cluster!";

		send(client_fd, dummyResponse.c_str(), dummyResponse.length(), 0);

		// std::cout << "--- Got request from FD " << client_fd << " ---" << std::endl;
		// ! Temperorly ! Delete after Sasha creates Request class
		// this->closeConnection(pollfd_index);
		return false;
	} else if (byte_reads == 0){
		std::cout << "Client (FD " << client_fd << ") closed connection." << std::endl;
		this->closeConnection(pollfd_index);
		return true;
	} else {
		std::cerr << "Recv error on FD " << client_fd << ": " << strerror(errno) << std::endl;
		this->closeConnection(pollfd_index);
		return true;
	}

}

void Cluster::closeConnection(size_t pollfd_index)
{
	if (pollfd_index >= _pollfds.size())
		return;
	int fd = _pollfds[pollfd_index].fd;
	if (fd >= 0)
		close(fd);
	_pollfds.erase(_pollfds.begin() + pollfd_index);

	// TO DO: After Request implimentation
	// _requests.erase(fd);
	std::cout << "[Cluster] Connection closed on FD: " << fd << std::endl;
}
