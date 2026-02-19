#include "Cluster.hpp"

Cluster::Cluster(){}
Cluster::Cluster(const std::vector<ServerConfig>& config) : _config_data(config) {}
Cluster::Cluster(const Cluster& other){}
Cluster& Cluster::operator=(const Cluster& other){}
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
	_fd_table.clear();
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
		sockaddr_in address{};
		address.sin_family = AF_INET;
		address.sin_port = htons(port);
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

		addFD(socket_fd, FD_LISTENER, -1, 0);
		_listen_sockets[socket_fd] = port;

		std::cout << "Listening on port " << port << " (fd: " << socket_fd << ")" << std::endl;
	}
}
void	Cluster::run()
{
	std::cout << "--- Server is starting the event loop ---" << std::endl;

	while (true){
		int ret = poll(_pollfds.data(), _pollfds.size(), 1000);
		if (ret < 0){
			if (errno == EINTR)
				continue;
			throw std::runtime_error("Poll filed: " + std::string(strerror(errno)));
		}

		handleTimeout();

		for (int i = 0; i < static_cast<int>(_pollfds.size()); ++i){
			int fd = _pollfds[i].fd;
			short revents = _pollfds[i].revents;

			if (revents == 0)
				continue;
			updateActivity(fd);

			if (revents & POLLIN){
				if (_fd_table[fd].type == FD_LISTENER){
					acceptNewConnection(fd);
				} else if (_fd_table[fd].type == FD_CLIENT){
					if (handleClientRequest(fd) == true){
						--i;
						continue;
					}
				}
			}

			if (revents & POLLOUT){
				if (_fd_table[fd].type == FD_CLIENT){
					if (handleClientResponse(fd) == true){
						--i;
						continue;
					}
				}
			}

			if (revents & (POLLHUP | POLLERR | POLLNVAL)){
				closeConnection(fd);
				--i;
				continue;
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
	int port = _listen_sockets.at(listen_fd);

	int timeout = 60;
	for (const auto& config : _config_data){
		if (config.port == port){
			timeout = config.client_timeout;
			break;
		}
	}
	addFD(client_fd, FD_CLIENT, -1, timeout);
	std::cout << "New client connected: FD " << client_fd << std::endl;
}


bool Cluster::handleClientRequest(int fd)
{
	char buffer[4096];

	ssize_t byte_reads = recv(fd, buffer, sizeof(buffer) - 1, 0);

	if (byte_reads > 0){
		FDMetadata& data = _fd_table.at(fd);
		data.last_activity = time(NULL);

		data.request.consume(std::string(buffer, byte_reads));
		if (data.request.isFinished()) {
			if (data.request.getState() == Request::ERROR){
				int error_code = data.request.getErrorCode();
				Logger::error("Request error " + std::to_string(error_code) + " on FD " + std::to_string(fd));
				data.response = generateErrorResponse(error_code, data.config_index);

				data.client_state = STATE_WRITING;
				updatePollEvents(fd, POLLOUT);
				data.client_state = STATE_PROCESSING;
			}
			else {
				Logger::info("Request " + data.request.getMethod() + " " +
				data.request.getPath() + " fully recieved [FD " + std::to_string(fd) + "]");
				data.client_state = STATE_PROCESSING;
				this->processRequest(fd);
			}
		}
		return false;
	}
	else if (byte_reads == 0){
		Logger::info("Client closed connection [FD " + std::to_string(fd) + "]");
		closeConnection(fd);
		return true;
	}
	else{
		if (errno != EWOULDBLOCK && errno != EAGAIN){
			Logger::error("Recv failed [FD " + std::to_string(fd) + "]");
			closeConnection(fd);
			return true;
		}
		return false;
	}
}

void Cluster::closeConnection(int fd)
{
	if (_fd_table.count(fd)){
		_fd_table.at(fd).write_buffer.clear();
	}
	removeFD(fd);

	// TO DO: When i will stat work with map of requests
	// _requests.erase(fd);
	std::cout << "[Cluster] Connection closed on FD: " << fd << std::endl;
}

void Cluster::updatePollEvents(int fd, short events)
{
	for (auto& pfd : _pollfds) {
		if (pfd.fd == fd) {
			pfd.events = events;
			break;
		}
	}
}

bool Cluster::handleClientResponse(int fd)
{
	std::string& response = _fd_table[fd].write_buffer;
	if (response.empty()){
		updatePollEvents(fd, POLLIN);
		return false;
	}
	// TO DO: Check if everything was sent or not. If yes delete only sent data
	ssize_t bytes_sent = send(fd, response.c_str(), response.length(), 0);

	if (bytes_sent > 0){
		response.erase(0, bytes_sent);
		if (response.empty()){
			updatePollEvents(fd, POLLIN);
		}
		return false;
	} else if (bytes_sent == 0){
		closeConnection(fd);
		return true;
	} else {
		if (errno != EWOULDBLOCK && errno != EAGAIN) {
			closeConnection(fd);
			return true;
		}
		return true;
	}
}

void Cluster::addFD(int fd, FDType type, int client_ref, int timeout)
{
	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
		return;
	struct pollfd pfd;
	pfd.fd = fd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	_pollfds.push_back(pfd);

	FDMetadata metadata;
	metadata.fd = fd;
	metadata.type = type;
	metadata.client_fd = client_ref;
	metadata.last_activity = time(NULL);
	metadata.timeout_value = timeout;
	metadata.is_ready_to_close = false;

	_fd_table[fd] = metadata;
}

void Cluster::removeFD(int fd)
{
	for (auto it = _pollfds.begin(); it != _pollfds.end(); ++it){
		if (it->fd == fd){
			_pollfds.erase(it);
			break;
		}
	}
	_fd_table.erase(fd);

	close(fd);
}

void Cluster::updateActivity(int fd)
{
	if (auto it = _fd_table.find(fd); it != _fd_table.end()){
		it->second.last_activity = time(NULL);
	}
}

void Cluster::handleTimeout()
{
	time_t now = time(NULL);
	std::vector<int> fd_to_close;

	for (auto& [fd, metadata] : _fd_table){
		if (metadata.type == FD_LISTENER){
			continue;
		}
		if (now - metadata.last_activity > metadata.timeout_value){
			std::cout << "Timeout reached for FD: " << fd << std::endl;
			fd_to_close.push_back(fd);
		}
	}
	for (int fd : fd_to_close)
		closeConnection(fd);
}

Response Cluster::generateErrorResponse(int code, int config_index) {
	Response res;
	const ServerConfig& config = _config_data[config_index];

	if (config.error_pages.count(code)) {
		std::string path = config.error_pages.at(code);
		std::string custom_body = loadFile(path);
		if (!custom_body.empty()) {
			res.setStatusCode(code);
			res.setBody(custom_body);
			res.addHeader("Content-Type", "text/html");
			return res;
		}
	}

	res.makeDefaultError(code);
	return res;
}
