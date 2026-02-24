#include "Cluster.hpp"

Cluster::Cluster(){}
Cluster::Cluster(const std::vector<ServerConfig>& config) : _config_data(config) {}
Cluster::~Cluster()
{
	std::vector<int> fds_to_close;
	for (const auto& pfd : _pollfds)
	{
		if (pfd.fd >= 0)
			fds_to_close.push_back(pfd.fd);
	}
	for (int fd : fds_to_close)
		removeFD(fd);
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

		Logger::info("Listening on port " + std::to_string(port) + " (fd: " + std::to_string(socket_fd) + ")");
		std::cout << "http://localhost:" << port << std::endl;
	}
}
void	Cluster::run()
{
	Logger::info("--- Server is starting the event loop ---");

	std::vector<std::pair<int, short>> events;

	while (true){
		int ret = poll(_pollfds.data(), _pollfds.size(), 1000);
		if (ret < 0){
			if (errno == EINTR)
				continue;
			throw std::runtime_error("Poll filed: " + std::string(strerror(errno)));
		}

		events.clear();
		for (const auto& pfd : _pollfds){
			if (pfd.revents != 0)
				events.emplace_back(pfd.fd, pfd.revents);
		}
		handleTimeout();

		for (const auto& [fd, revents] : events){
			if (_fd_table.find(fd) == _fd_table.end())
				continue;

			if (revents & (POLLERR | POLLNVAL)){
				Logger::warning("Poll error on FD " + std::to_string(fd));
				closeConnection(fd);
				continue;
			}
			updateActivity(fd);


			// READABLE
			if (revents & POLLIN){
				auto it = _fd_table.find(fd);
				if (it == _fd_table.end())
					continue;

				if (it->second.type == FD_LISTENER){
					acceptNewConnection(fd);
				} else if (it->second.type == FD_CLIENT){
					if (handleClientRequest(fd))
						continue;
				}
			}

			// WRITABLE
			if (revents & POLLOUT){
				auto it = _fd_table.find(fd);
				if (it == _fd_table.end())
					continue;

				if (it->second.type == FD_CLIENT){
					if (handleClientResponse(fd))
						continue;
				}
			}


			if (revents & POLLHUP){
				if (_fd_table.find(fd) != _fd_table.end())
					closeConnection(fd);
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
		if (errno == EWOULDBLOCK || errno == EAGAIN)
			return;
		std::cerr << "Warning: accept() failed: " << strerror(errno) << std::endl;
		return;
	}
	int port = _listen_sockets.at(listen_fd);

	int timeout = DEFAULT_CLIENT_TIMEOUT;
	int config_index = -1;
	unsigned long max_body = DEFAULT_MAX_BODY_SIZE;

	for (int idx = 0; idx < static_cast<int>(_config_data.size()); ++idx){
		if (_config_data[idx].port == port){
			config_index = idx;
			timeout = _config_data[idx].client_timeout;
			max_body = _config_data[idx].client_max_body_size;
			break;
		}
	}
	if (config_index < 0){
		Logger::error("No config found for port " + std::to_string(port));
		close(client_fd);
		return;
	}
	addFD(client_fd, FD_CLIENT, -1, timeout);

	// Init data that addFD() doesn't know;
	FDMetadata& data = _fd_table[client_fd];
	data.port = port;
	data.config_index = config_index;
	data.client_state = STATE_READING;
	data.request.setMaxBodySize(max_body);

	std::cout << "New client connected: FD " << client_fd << std::endl;
}


bool Cluster::handleClientRequest(int fd)
{
	char buffer[RECV_BUFFER_SIZE];

	ssize_t byte_reads = recv(fd, buffer, sizeof(buffer) - 1, 0);

	if (byte_reads > 0){
		FDMetadata& data = _fd_table.at(fd);
		data.last_activity = time(NULL);

		data.request.consume(std::string(buffer, static_cast<size_t>(byte_reads)));

		if (!data.request.isFinished())
			return false;

		if (data.request.getState() == Request::ERROR){
			int error_code = data.request.getErrorCode();
			Logger::error("Request error " + std::to_string(error_code) + " on FD " + std::to_string(fd));
			data.response = generateErrorResponse(error_code, data.config_index);
			data.response.prepare();
			data.client_state = STATE_WRITING;
			updatePollEvents(fd, POLLOUT);
		}
		else {
			Logger::info("Request " + data.request.getMethod() + " " +
			data.request.getPath() + " fully recieved [FD " + std::to_string(fd) + "]");
			data.client_state = STATE_PROCESSING;
			// TO DO: Handle error pages;
			data.response = RequestHandler::handleRequest(data.request, _config_data[data.config_index]);
			data.response.prepare();
			data.client_state = STATE_WRITING;
			updatePollEvents(fd, POLLOUT);
		}
		return false;
	}
	if (byte_reads == 0){
		Logger::info("Client closed connection [FD " + std::to_string(fd) + "]");
		closeConnection(fd);
		return true;
	}
	if (errno != EWOULDBLOCK && errno != EAGAIN){
		Logger::error("recv() failed [FD " + std::to_string(fd) + "]: " + std::string(strerror(errno)));
		closeConnection(fd);
		return true;
	}
	return false;
}

void Cluster::closeConnection(int fd)
{
	if (_fd_table.count(fd)){
		_fd_table.at(fd).write_buffer.clear();
	}

	removeFD(fd);
	Logger::info("[Cluster] Connection closed on [FD " + std::to_string(fd) + "]");
}

void Cluster::updatePollEvents(int fd, short events)
{
	for (auto& pfd : _pollfds) {
		if (pfd.fd == fd) {
			pfd.events = events;
			return;
		}
	}
}

bool Cluster::handleClientResponse(int fd)
{
	// std::string& response = _fd_table[fd].write_buffer;
	FDMetadata& data = _fd_table.at(fd);
	if (data.response.getRemainingSize() == 0){
		closeConnection(fd);
		return true;
	}
	// TO DO: Check if everything was sent or not. If yes delete only sent data
	ssize_t bytes_sent = send(fd, data.response.getUnsentData(), data.response.getRemainingSize(), MSG_NOSIGNAL);

	if (bytes_sent > 0){
		// TO DO: Update in updateSentBytes size_t to ssize_t
		data.response.updateSentBytes(static_cast<size_t>(bytes_sent));
		if (data.response.isFinished()){
			Logger::info("Response sent successfully [FD " + std::to_string(fd) + "]");
			closeConnection(fd);
			return true;
		}
		return false;
	}
	if (bytes_sent == 0){
		closeConnection(fd);
		return true;
	}
	if (errno != EWOULDBLOCK && errno != EAGAIN) {
		Logger::error("Send failed [FD " + std::to_string(fd) + "]");
		closeConnection(fd);
		return true;
	}
	return false;
}

void Cluster::addFD(int fd, FDType type, int client_ref, int timeout)
{
	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0){
		Logger::error("fcntl(O_NONBLOCK) failed for FD " + std::to_string(fd)
		+ ": " + std::string(strerror(errno)));
		close(fd);
		return;
	}
	struct pollfd pfd;
	pfd.fd = fd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	_pollfds.push_back(pfd);

	FDMetadata metadata = {};
	metadata.fd = fd;
	metadata.type = type;
	metadata.client_state = STATE_READING;
	metadata.client_fd = client_ref;
	metadata.last_activity = time(NULL);
	metadata.timeout_value = timeout;
	metadata.is_ready_to_close = false;
	metadata.port = -1;
	metadata.config_index = -1;

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
		if (metadata.timeout_value <= 0)
			continue;
		if (now - metadata.last_activity > metadata.timeout_value){
			Logger::info("Timeout [FD " + std::to_string(fd) + "]");
			fd_to_close.push_back(fd);
		}
	}
	for (int fd : fd_to_close)
		closeConnection(fd);
}

Response Cluster::generateErrorResponse(int code, int config_index) {
	Response res;

	if (config_index >= 0 && config_index < static_cast<int>(_config_data.size())){

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
	}

	res.makeDefaultError(code);
	return res;
}
