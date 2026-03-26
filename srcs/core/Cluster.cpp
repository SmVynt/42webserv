#include "Cluster.hpp"

static const size_t			RECV_BUFFER_SIZE		= 65536;

// ============================================================================
// Lifecycle
// ============================================================================

Cluster::Cluster() : _fd_generation(0), _shutdown(false) {}

Cluster::Cluster(const std::vector<ServerConfig>& config)
	: _config_data(config), _fd_generation(0), _shutdown(false) {}

Cluster::~Cluster()
{
	// First pass: kill CGI children before closing pipes,
	// otherwise executors may write to already-closed FDs
	for (auto& [fd, meta] : _fd_table)
	{
		if (meta.cgi_executor)
		{
			meta.cgi_executor->killChildProcess();
			delete meta.cgi_executor;
			meta.cgi_executor = nullptr;
		}
	}
	// Second pass: snapshot FDs then close — removeFD() mutates _pollfds
	std::vector<int> fds_to_close;
	for (const auto& pfd : _pollfds)
	{
		if (pfd.fd >= 0)
			fds_to_close.push_back(pfd.fd);
	}
	for (int fd : fds_to_close)
		removeFD(fd);
}

void Cluster::setupCluster()
{
	for (const auto& pfd : _pollfds)
	{
		if (pfd.fd >= 0)
			close(pfd.fd);
	}
	_pollfds.clear();
	_fd_table.clear();
	_listen_sockets.clear();

	// Multiple server blocks may share the same host:port — deduplicate
	std::set<std::pair<std::string, int>> bound_addresses;

	int error_code = 0;
	for (const auto& config : _config_data)
	{
		std::string host = config.host.empty() ? "0.0.0.0" : config.host;
		int port = config.port;

		if (bound_addresses.count({host, port}))
			continue;

		int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (socket_fd < 0)
		{
			error_code = errno;
			throw std::runtime_error("Socket creation failed: " + std::string(strerror(error_code)));
		}
		// SO_REUSEADDR lets us rebind immediately after restart without TIME_WAIT
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
		if (host == "0.0.0.0") {
			address.sin_addr.s_addr = INADDR_ANY;
		} else {
			struct addrinfo hints{}, *res = nullptr;
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_STREAM;
			if (getaddrinfo(host.c_str(), nullptr, &hints, &res) == 0 && res) {
				address.sin_addr = reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr;
				freeaddrinfo(res);
			} else {
				if (res) freeaddrinfo(res);
				close(socket_fd);
				throw std::runtime_error("getaddrinfo failed for host: " + host);
			}
		}

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
		bound_addresses.insert({host, port});

		Logger::info("Listening on port " + host + ":" + std::to_string(port) + " (fd: " + std::to_string(socket_fd) + ")");
		std::cout << "http://localhost:" << port << std::endl;
	}
}

void Cluster::requestShutdown()
{
	_shutdown = true;
}

// ============================================================================
// Event Loop
// ============================================================================

void Cluster::run()
{
	Logger::info("--- Server is starting the event loop ---");

	struct PollEvent { int fd; short revents; uint64_t gen; };
	std::vector<PollEvent> events;

	while (!_shutdown){
		// Compact dead entries (fd == -1) left by removeFD / removeFDNoClose
		_pollfds.erase(
			std::remove_if(_pollfds.begin(), _pollfds.end(),
				[](const pollfd& p){ return p.fd < 0; }),
			_pollfds.end());

		// 1 s timeout: guarantees periodic handleTimeout() even with no traffic
		int ret = poll(_pollfds.data(), _pollfds.size(), 1000);
		if (ret < 0){
			// EINTR: signal (e.g. SIGINT) interrupted poll — safe to retry
			if (errno == EINTR)
				continue;
			throw std::runtime_error("Poll filed: " + std::string(strerror(errno)));
		}

		// Snapshot events with generation stamps before dispatching.
		// Handlers may close/add FDs mid-iteration; the snapshot ensures we
		// never process a stale event from a recycled FD number.
		events.clear();
		for (const auto& pfd : _pollfds){
			if (pfd.fd >= 0 && pfd.revents != 0) {
				auto it = _fd_table.find(pfd.fd);
				uint64_t gen = (it != _fd_table.end()) ? it->second.generation : 0;
				events.push_back({pfd.fd, pfd.revents, gen});
			}
		}
		handleTimeout();

		for (const auto& ev : events){
			int fd = ev.fd;
			short revents = ev.revents;

			// FD may have been removed by a previous handler in this iteration
			auto meta_it = _fd_table.find(fd);
			if (meta_it == _fd_table.end())
			{
				if (revents & POLLNVAL)
				{
					for (auto& p : _pollfds)
						if (p.fd == fd)
							p.fd = -1;
				}
				continue;
			}
			// Generation mismatch: this FD number was recycled since the snapshot
			if (meta_it->second.generation != ev.gen)
				continue;

			if (revents & POLLERR){
				Logger::warning("Poll error on FD " + std::to_string(fd));
				closeConnection(fd);
				continue;
			}
			if (revents & POLLNVAL){
				Logger::warning("Poll NVAL on FD " + std::to_string(fd));
				removeFDNoClose(fd);
				continue;
			}
			updateActivity(fd);

			if (revents & POLLIN){
				// Re-lookup: previous POLLERR/POLLNVAL handler may have removed this FD
				auto it = _fd_table.find(fd);
				if (it == _fd_table.end())
					continue;

				if (it->second.type == FD_LISTENER)
					acceptNewConnection(fd);
				else if (it->second.type == FD_CLIENT)
				{
					if (handleClientRequest(fd))
						continue;
				}
				else if (it->second.type == FD_CGI_OUT)
					handleCgiRead(fd);
			}

			if (revents & POLLOUT){
				auto it = _fd_table.find(fd);
				if (it == _fd_table.end())
					continue;

				if (it->second.type == FD_CLIENT)
				{
					if (handleClientResponse(fd))
						continue;
				}
				else if (it->second.type == FD_CGI_IN)
					handleCgiWrite(fd);
			}

			if (revents & POLLHUP){
				// CGI_OUT: child closed stdout — drain remaining data before finalising
				auto it_hup = _fd_table.find(fd);
				if (it_hup != _fd_table.end()){
					if (it_hup->second.type == FD_CGI_OUT)
						handleCgiRead(fd);
					else
						closeConnection(fd);
				}
			}
		}
	}

	Logger::info("Shutdown requested. Server will stop accepting new connections and close existing ones.");
}

// ============================================================================
// FD Management
// ============================================================================

void Cluster::addFD(int fd, FDType type, int client_ref, int timeout)
{
	// Subject requirement: every FD must be non-blocking (poll-driven I/O only)
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
	// Monotonic generation counter — prevents stale event dispatch after FD reuse
	metadata.generation = ++_fd_generation;
	metadata.fd = fd;
	metadata.type = type;
	metadata.client_state = STATE_READING;
	metadata.client_fd = client_ref;
	metadata.last_activity = time(NULL);
	metadata.timeout_value = timeout;
	metadata.timeout_reading_value = 30;
	metadata.needs_continue = false;
	metadata.cgi_executor = nullptr;
	metadata.session_ptr = nullptr;
	metadata.port = -1;
	metadata.config_index = -1;
	metadata.is_new_session = false;

	_fd_table[fd] = metadata;
}

void Cluster::removeFD(int fd)
{
	// Mark as -1 for lazy compaction at the start of the next poll() iteration
	for (auto& pfd : _pollfds)
		if (pfd.fd == fd)
			pfd.fd = -1;
	if (fd >= 0)
		close(fd);
	_fd_table.erase(fd);
}

void Cluster::removeFDNoClose(int fd)
{
	for (auto& pfd : _pollfds)
		if (pfd.fd == fd)
			pfd.fd = -1;
	_fd_table.erase(fd);
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

void Cluster::updateActivity(int fd)
{
	if (auto it = _fd_table.find(fd); it != _fd_table.end())
		it->second.last_activity = time(NULL);
}

void Cluster::handleTimeout()
{
	time_t now = time(NULL);
	std::vector<int> fd_to_close;

	// Can't close inside iteration (invalidates map iterators) — collect first
	for (auto& [fd, metadata] : _fd_table){
		if (metadata.type == FD_LISTENER)
			continue;
		if (metadata.timeout_value <= 0)
			continue;
		// Two timeout tiers: general (any FD) and stricter reading-only (408)
		if (now - metadata.last_activity > metadata.timeout_value){
			Logger::info("Timeout [FD " + std::to_string(fd) + "]");
			fd_to_close.push_back(fd);
		} else if (metadata.type == FD_CLIENT && metadata.client_state == STATE_READING
				&& metadata.timeout_reading_value > 0
				&& now - metadata.last_activity > metadata.timeout_reading_value) {
			Logger::info("Reading timeout [FD " + std::to_string(fd) + "]");
			metadata.response = generateErrorResponse(408, metadata.config_index);
			metadata.response.prepare();
			metadata.client_state = STATE_WRITING;
			updatePollEvents(fd, POLLOUT);
		}
	}
	for (int fd : fd_to_close)
		closeConnection(fd);

	cleanupExpiredSessions();
}

void Cluster::cleanupExpiredSessions()
{
	int session_ttl = 10;
	if (!_config_data.empty() && _config_data[0].session_timeout > 0)
		session_ttl = _config_data[0].session_timeout;
	for (auto it = _active_sessions.begin(); it != _active_sessions.end(); ) {
		if (it->second.isExpired(session_ttl)) {
			Logger::info("Session expired: " + it->second.getId());
			it = _active_sessions.erase(it);
		} else {
			++it;
		}
	}
}

// ============================================================================
// Connection Management
// ============================================================================

void Cluster::acceptNewConnection(int listen_fd)
{
	struct sockaddr_in client_addr{};
	socklen_t client_len = sizeof(client_addr);

	int client_fd = accept(listen_fd, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);

	if (client_fd < 0){
		// Spurious wakeup: poll() reported POLLIN but no pending connection
		if (errno == EWOULDBLOCK || errno == EAGAIN)
			return;
		std::cerr << "Warning: accept() failed: " << strerror(errno) << std::endl;
		return;
	}
	int port = _listen_sockets.at(listen_fd);

	int timeout = 0;
	int config_index = -1;
	unsigned long max_body = 0;

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

	FDMetadata& data = _fd_table[client_fd];
	data.port = port;
	data.config_index = config_index;
	data.client_state = STATE_READING;
	data.request.setMaxBodySize(max_body);
	char ip_str[NI_MAXHOST];
	getnameinfo(reinterpret_cast<struct sockaddr*>(&client_addr), client_len,
				ip_str, sizeof(ip_str), nullptr, 0, NI_NUMERICHOST);
	data.request.setClientIP(ip_str);

	std::cout << "New client connected: FD " << client_fd << std::endl;
}

std::vector<int> Cluster::collectCgiPipes(int client_fd)
{
	std::vector<int> pipes;
	for (const auto& [fd, meta] : _fd_table)
	{
		if ((meta.type == FD_CGI_OUT || meta.type == FD_CGI_IN) && meta.client_fd == client_fd)
			pipes.push_back(fd);
	}
	return pipes;
}

void Cluster::closeConnection(int fd)
{
	auto it = _fd_table.find(fd);
	if (it == _fd_table.end())
	{
		for (auto& pfd : _pollfds)
			if (pfd.fd == fd)
				pfd.fd = -1;
		return;
	}

	// Branch 1: closing a CGI pipe — notify the owning client with 504
	if (it->second.type == FD_CGI_OUT || it->second.type == FD_CGI_IN)
	{
		int client_fd = it->second.client_fd;
		std::vector<int> orphan_pipes = collectCgiPipes(client_fd);

		if (_fd_table.count(client_fd))
		{
			FDMetadata& cl = _fd_table.at(client_fd);
			if (cl.cgi_executor)
			{
				// NoClose: executor destructor owns the underlying pipe FDs
				for (int p : orphan_pipes)
					removeFDNoClose(p);
				cl.cgi_executor->killChildProcess();
				delete cl.cgi_executor;
				cl.cgi_executor = nullptr;
			}
			else
			{
				for (int p : orphan_pipes)
					removeFD(p);
			}
			cl.response = generateErrorResponse(504, cl.config_index);
			cl.response.prepare();
			cl.client_state = STATE_WRITING;
			updatePollEvents(client_fd, POLLOUT);
		}
		else
		{
			// Client already gone — just clean up the orphan pipes
			for (int p : orphan_pipes)
				removeFD(p);
		}
	}
	// Branch 2: closing a client FD — must tear down any running CGI first
	else
	{
		if (it->second.type == FD_CLIENT && it->second.cgi_executor != nullptr)
		{
			Logger::info("Cleaning up CGI executor for client FD " + std::to_string(fd));
			std::vector<int> orphan_pipes = collectCgiPipes(fd);
			for (int p : orphan_pipes)
				removeFDNoClose(p);
			it->second.cgi_executor->killChildProcess();
			delete it->second.cgi_executor;
			it->second.cgi_executor = nullptr;
		}
		removeFD(fd);
	}
	Logger::info("[Cluster] Connection closed on [FD " + std::to_string(fd) + "]");
}

void Cluster::resetConnection(int fd)
{
	FDMetadata& data = _fd_table.at(fd);

	// Save socket-level fields that must survive keep-alive reset —
	// replacing Request/Response would destroy them
	int saved_port = data.port;
	int saved_timeout = data.timeout_value;
	int saved_config = data.config_index;
	unsigned long saved_max_body = _config_data[saved_config].client_max_body_size;

	data.request = Request();
	data.response = Response();
	data.client_state = STATE_READING;

	data.port = saved_port;
	data.timeout_value = saved_timeout;
	data.config_index = saved_config;
	data.last_activity = time(NULL);
	data.request.setMaxBodySize(saved_max_body);
	data.is_new_session = false;
	data.needs_continue = false;
	data.session_id.clear();
	data.session_ptr = nullptr;

	updatePollEvents(fd, POLLIN);

	Logger::info("Keep-Alive: reset for next request [FD " + std::to_string(fd) + "]");
}

// ============================================================================
// Request Processing
// ============================================================================

bool Cluster::handleClientRequest(int fd)
{
	char buffer[RECV_BUFFER_SIZE];
	ssize_t byte_reads = recv(fd, buffer, sizeof(buffer), 0);

	if (byte_reads > 0){
		FDMetadata& data = _fd_table.at(fd);
		data.last_activity = time(NULL);
		data.request.consume(buffer, static_cast<size_t>(byte_reads));

		if (!data.request.isFinished()) {
			// RFC 7231 §5.1.1: respond with "100 Continue" before the client sends the body
			if (data.request.needsContinue()) {
				data.needs_continue = true;
				updatePollEvents(fd, POLLOUT);
			}
			return false;
		}
		data.needs_continue = false;
		processCompletedRequest(fd, data);
		return false;
	}
	// recv() == 0 means clean TCP FIN — client closed the connection
	if (byte_reads == 0){
		Logger::info("Client closed connection [FD " + std::to_string(fd) + "]");
		closeConnection(fd);
		return true;
	}
	Logger::error("recv() failed [FD " + std::to_string(fd) + "]");
	closeConnection(fd);
	return true;
}

void Cluster::processCompletedRequest(int fd, FDMetadata& data)
{
	if (data.request.getState() == Request::ERROR){
		int error_code = data.request.getErrorCode();
		Logger::error("Request error " + std::to_string(error_code) + " on FD " + std::to_string(fd));
		data.response = generateErrorResponse(error_code, data.config_index);
		data.response.prepare();
		data.client_state = STATE_WRITING;
		updatePollEvents(fd, POLLOUT);
		return;
	}

	// RFC 7230 §5.4: Host header may include port — strip it for name matching
	std::string host = data.request.getHeaders("host");
	size_t colon = host.find(':');
	if (colon != std::string::npos)
		host = host.substr(0, colon);

	// Virtual hosting: pick the server block whose server_name matches Host
	int resolved = resolveServerConfig(data.port, host);
	if (resolved >= 0)
		data.config_index = resolved;

	const ServerConfig& config = _config_data[data.config_index];

	resolveSession(data);

	Logger::info("Request " + data.request.getMethod() + " " +
				data.request.getPath() + " fully received [FD " + std::to_string(fd) + "]");

	const Location* loc = RequestHandler::resolveLocation(
		data.request.getPath(), data.request.getMethod(), config);

	Logger::debug("resolveLocation returned loc=" + std::string(loc ? "found" : "nullptr"));
	if (loc)
		Logger::debug("location path='" + loc->path + "'");
	// Location-level body limit overrides the server-level default
	if (loc && loc->client_max_body_size.has_value())
		data.request.setMaxBodySize(loc->client_max_body_size.value());
	else if (loc)
		data.request.setMaxBodySize(config.client_max_body_size);

	if (validateBodySize(fd, data, loc, config))
		return;

	if (loc && RequestHandler::isCgiRequest(data.request, *loc))
		launchCgiRequest(fd, data, *loc, config);
	else
		handleStaticRequest(fd, data, config);
}

bool Cluster::validateBodySize(int fd, FDMetadata& data, const Location* loc, const ServerConfig& config)
{
	unsigned long max_size = (loc && loc->client_max_body_size.has_value())
		? loc->client_max_body_size.value()
		: config.client_max_body_size;
	unsigned long body_size = data.request.getBody().size();

	// Content-Length may declare more data than already received (streaming) —
	// reject early before the full body arrives
	if (data.request.getHeaders().count("content-length")) {
		try {
			unsigned long cl = std::stoul(data.request.getHeaders("content-length"));
			if (cl > max_size)
				body_size = cl;
		} catch (...) {}
	}

	if (body_size > max_size) {
		data.response = generateErrorResponse(413, data.config_index);
		data.response.prepare();
		data.client_state = STATE_WRITING;
		updatePollEvents(fd, POLLOUT);
		return true;
	}
	return false;
}

void Cluster::launchCgiRequest(int fd, FDMetadata& data, const Location& loc, const ServerConfig& config)
{
	Logger::info("Launching CGI for FD " + std::to_string(fd));
	data.cgi_executor = RequestHandler::handleCgi(data.request, loc, config);

	if (!data.cgi_executor || data.cgi_executor->hasError())
	{
		data.response = generateErrorResponse(500, data.config_index);
		if (data.is_new_session)
			data.response.addHeader("Set-Cookie", "session_id=" + data.session_id + "; Path=/");
		data.response.prepare();
		data.client_state = STATE_WRITING;
		updatePollEvents(fd, POLLOUT);
		if (data.cgi_executor)
		{
			delete data.cgi_executor;
			data.cgi_executor = nullptr;
		}
	}
	else
	{
		data.client_state = STATE_PROCESSING;
		// Register CGI stdout pipe for reading
		int pipe_out = data.cgi_executor->getOutputFd();
		addFD(pipe_out, FD_CGI_OUT, fd, config.client_timeout);
		_fd_table[pipe_out].session_ptr = data.session_ptr;
		// Register CGI stdin pipe for writing POST body (if present)
		int pipe_in = data.cgi_executor->getInputFd();
		if (pipe_in >= 0)
		{
			addFD(pipe_in, FD_CGI_IN, fd, config.client_timeout);
			_fd_table[pipe_in].session_ptr = data.session_ptr;
			updatePollEvents(pipe_in, POLLOUT);
		}
		// Suspend client FD — no direct I/O until CGI completes
		updatePollEvents(fd, 0);
	}
}

void Cluster::handleStaticRequest(int fd, FDMetadata& data, const ServerConfig& config)
{
	data.client_state = STATE_PROCESSING;
	data.response = RequestHandler::handleRequest(data.request, config);
	Logger::warning("Status code" + std::to_string(data.response.getStatusCode()));
	if (data.response.getStatusCode() != 200 && data.response.getStatusCode() != 201)
		data.response = generateErrorResponse(data.response.getStatusCode(), data.config_index);
	// RFC 7231 §4.3.2: HEAD response must have no body but keep Content-Length
	if (data.request.getMethod() == "HEAD")
		data.response.setBody("");
	if (data.is_new_session)
		data.response.addHeader("Set-Cookie", "session_id=" + data.session_id + "; Path=/");
	data.response.prepare();
	data.client_state = STATE_WRITING;
	updatePollEvents(fd, POLLOUT);
}

int Cluster::resolveServerConfig(int port, const std::string& host)
{
	int default_config = -1;

	for (int idx = 0; idx < static_cast<int>(_config_data.size()); ++idx)
	{
		if (_config_data[idx].port != port)
			continue;
		if (default_config < 0)
			default_config = idx;
		for (const auto& name : _config_data[idx].server_names)
		{
			if (name == host)
				return idx;
		}
	}
	return default_config;
}

void Cluster::resolveSession(FDMetadata& data)
{
	// Parse "session_id=<value>" from the Cookie header (RFC 6265)
	std::string cookie_header = data.request.getHeaders("cookie");
	std::string session_id;

	if (!cookie_header.empty()) {
		size_t start_pos = cookie_header.find("session_id=");
		if (start_pos != std::string::npos) {
			start_pos += 11;
			size_t end_pos = cookie_header.find(';', start_pos);
			session_id = cookie_header.substr(start_pos,
				end_pos == std::string::npos ? end_pos : end_pos - start_pos);
		}
	}
	Session* session = findSessionById(session_id);
	if (session_id.empty() || !session) {
		Session new_session;
		session_id = new_session.getId();
		_active_sessions.insert({session_id, new_session});
		data.is_new_session = true;
	} else {
		session->touch();
	}
	data.session_id = session_id;
	// Safe: std::map guarantees pointer/reference stability after insert
	data.session_ptr = &_active_sessions.at(session_id);
}

Session* Cluster::findSessionById(const std::string& sessionId)
{
	auto it = _active_sessions.find(sessionId);
	if (it != _active_sessions.end())
		return &(it->second);
	return nullptr;
}

// ============================================================================
// Response Handling
// ============================================================================

bool Cluster::handleClientResponse(int fd)
{
	FDMetadata& data = _fd_table.at(fd);

	// Respond to "Expect: 100-continue" before switching back to POLLIN for the body
	if (data.needs_continue) {
		static const char cont[] = "HTTP/1.1 100 Continue\r\n\r\n";
		// MSG_NOSIGNAL: prevent SIGPIPE if client already disconnected (Linux)
		send(fd, cont, sizeof(cont) - 1, MSG_NOSIGNAL);
		data.needs_continue = false;
		updatePollEvents(fd, POLLIN);
		return false;
	}

	if (data.response.getRemainingSize() == 0){
		closeConnection(fd);
		return true;
	}
	ssize_t bytes_sent = send(fd, data.response.getUnsentData(), data.response.getRemainingSize(), MSG_NOSIGNAL);

	if (bytes_sent > 0){
		data.response.updateSentBytes(static_cast<size_t>(bytes_sent));
		if (data.response.isFinished()){
			Logger::info("Response sent successfully [FD " + std::to_string(fd) + "]");
			if (data.request.shouldKeepAlive()){
				resetConnection(fd);
				return false;
			}
			closeConnection(fd);
			return true;
		}
		return false;
	}
	if (bytes_sent == 0){
		closeConnection(fd);
		return true;
	}
	Logger::error("Send failed [FD " + std::to_string(fd) + "]");
	closeConnection(fd);
	return true;
}

Response Cluster::generateErrorResponse(int code, int config_index)
{
	Response res;

	if (config_index >= 0 && config_index < static_cast<int>(_config_data.size())){
		const ServerConfig& config = _config_data[config_index];
		if (config.error_pages.count(code)) {
			std::string path = config.error_pages.at(code);
			if (!path.empty() && path[0] == '/')
				path.erase(0, 1);
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

// ============================================================================
// CGI Pipeline
// ============================================================================

void Cluster::handleCgiRead(int cgi_fd)
{
	FDMetadata& cgi_data = _fd_table.at(cgi_fd);

	// Orphan pipe: client disconnected while CGI was still running
	if (_fd_table.find(cgi_data.client_fd) == _fd_table.end())
	{
		removeFD(cgi_fd);
		return;
	}

	char buffer[RECV_BUFFER_SIZE];
	ssize_t bytes = read(cgi_fd, buffer, sizeof(buffer));

	if (bytes > 0)
	{
		cgi_data.cgi_raw_output.append(buffer, bytes);
		updateActivity(cgi_fd);
		updateActivity(cgi_data.client_fd);
		Session* session = cgi_data.session_ptr;
		if (session)
			session->touch();
	}
	// read() == 0: child closed stdout (EOF) — finalise the CGI response
	else if (bytes == 0)
	{
		if (cgi_data.cgi_executor) {
			Logger::debug("CGI process exited with status " + std::to_string(cgi_data.cgi_executor->getExitStatus()) +
						" for client FD " + std::to_string(cgi_data.client_fd));
		}
		handleCgiEnd(cgi_fd);
	}
	else
	{
		Logger::error("CGI read error on pipe " + std::to_string(cgi_fd));
		handleCgiEnd(cgi_fd);
	}
}

void Cluster::handleCgiWrite(int cgi_in_fd)
{
	FDMetadata& pipe_data = _fd_table.at(cgi_in_fd);
	int client_fd = pipe_data.client_fd;

	if (_fd_table.find(client_fd) == _fd_table.end())
	{
		removeFD(cgi_in_fd);
		return;
	}

	const std::string& body = _fd_table.at(client_fd).request.getBody();
	if (body.empty() || pipe_data.cgi_write_offset >= body.size())
	{
		removeFD(cgi_in_fd);
		return;
	}

	ssize_t written = write(cgi_in_fd,
						body.c_str() + pipe_data.cgi_write_offset,
						body.size() - pipe_data.cgi_write_offset);
	if (written > 0)
	{
		pipe_data.cgi_write_offset += static_cast<size_t>(written);
		Session* session = pipe_data.session_ptr;
		if (session)
			session->touch();
		// All bytes written: close pipe to signal EOF to child's stdin
		if (pipe_data.cgi_write_offset >= body.size())
		{
			removeFD(cgi_in_fd);
			// Detach so executor won't double-close this FD in its destructor
			if (_fd_table.count(client_fd) && _fd_table.at(client_fd).cgi_executor)
				_fd_table.at(client_fd).cgi_executor->detachPipeFd(cgi_in_fd);
		}
	}
	else if (written == 0)
	{
		Logger::error("CGI write returned 0 on pipe " + std::to_string(cgi_in_fd));
		removeFD(cgi_in_fd);
		if (_fd_table.count(client_fd) && _fd_table.at(client_fd).cgi_executor)
			_fd_table.at(client_fd).cgi_executor->detachPipeFd(cgi_in_fd);
	}
	else
	{
		Logger::error("CGI write error on pipe " + std::to_string(cgi_in_fd));
		removeFD(cgi_in_fd);
		if (_fd_table.count(client_fd) && _fd_table.at(client_fd).cgi_executor)
			_fd_table.at(client_fd).cgi_executor->detachPipeFd(cgi_in_fd);
	}
}

void Cluster::handleCgiEnd(int cgi_fd)
{
	auto cgi_it = _fd_table.find(cgi_fd);
	if (cgi_it == _fd_table.end())
		return;

	int client_fd = cgi_it->second.client_fd;
	// Copy raw output before pipe cleanup — removeFDNoClose erases the metadata
	std::string raw_output = cgi_it->second.cgi_raw_output;

	std::vector<int> cgi_pipes = collectCgiPipes(client_fd);

	if (_fd_table.find(client_fd) != _fd_table.end())
	{
		FDMetadata& client_data = _fd_table.at(client_fd);

		client_data.response = RequestHandler::parseCgiOutput(raw_output);

		// Check if CGI child exited with non-zero status or internal error
		if (client_data.cgi_executor)
		{
			int cgi_state = client_data.cgi_executor->isComplete();
			if (client_data.cgi_executor->hasError() ||
				(cgi_state == 1 && client_data.cgi_executor->getExitStatus() != 0)) {
				Logger::error("Error code: " + CGIError::getStatusMessage(client_data.cgi_executor->getErrorType()));
				client_data.response.setBody(CGIError::getStatusMessage(client_data.cgi_executor->getErrorType()));
				client_data.response.setStatusCode(CGIError::getStatusCode(client_data.cgi_executor->getErrorType()));
			}
		}

		if (client_data.is_new_session)
			client_data.response.addHeader("Set-Cookie", "session_id=" + client_data.session_id + "; Path=/");
		client_data.response.prepare();

		client_data.client_state = STATE_WRITING;
		updatePollEvents(client_fd, POLLOUT);

		for (int p : cgi_pipes)
			removeFDNoClose(p);

		if (client_data.cgi_executor)
		{
			client_data.cgi_executor->killChildProcess();
			delete client_data.cgi_executor;
			client_data.cgi_executor = nullptr;
		}
	}
	else
	{
		for (int p : cgi_pipes)
			removeFD(p);
	}
	Logger::info("CGI processing complete for client FD " + std::to_string(client_fd));
}

// ============================================================================
// Global Helpers
// ============================================================================

Cluster*& cluster_reference()
{
	static Cluster* instance = nullptr;
	return instance;
}

void signal_handler(int sig)
{
	if (sig == SIGTERM || sig == SIGINT) {
		if (cluster_reference())
			cluster_reference()->requestShutdown();
	}
}
