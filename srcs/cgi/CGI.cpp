#include "CGI.hpp"

CGIconfig::CGIconfig(const std::string &path,
				   const std::string &query,
				   const std::string &post,
				   const ServerConfig &config) :
	script_path(path),
	query_string(query),
	post_data(post),
	// timeout(config.client_timeout),
	// max_output_size(config.client_max_body_size)
	_config(config)
	{}

CGIconfig::~CGIconfig() {}

CGIexecutor::CGIexecutor(const CGIconfig &CGIconfig) :
	_script_path(CGIconfig.script_path),
	_query_string(CGIconfig.query_string),
	_post_data(CGIconfig.post_data),
	// _timeout_seconds(CGIconfig.timeout),
	// _max_output_size(CGIconfig.max_output_size),
	_config(CGIconfig._config)
	{
	_error_type = CGIError::NO_ERROR;
	_start_time = time(NULL);
	_child_pid = -1;
	_pipe_out_fd = -1;
	_pipe_in_fd = -1;
	_exit_status = -1;
	_is_complete = false;
}

CGIexecutor::~CGIexecutor() {
	killChildProcess();
}

void	CGIexecutor::setQuery(const std::string &query)
{
	_query_string = query;
	_env_vars["QUERY_STRING"] = query;
}

void	CGIexecutor::setPostData(const std::string &data) {
	_post_data = data;
	_env_vars["CONTENT_LENGTH"] = std::to_string(data.length());
	setEnvKey("CONTENT_TYPE", "application/x-www-form-urlencoded");
	_env_vars["REQUEST_METHOD"] = "POST";
};

void	CGIexecutor::setHttpHeader(const std::string &name, const std::string &value) {
	// Convert HTTP header name to CGI format: Host -> HTTP_HOST
	std::string cgi_name = "HTTP_" + name;
	for (size_t i = 0; i < cgi_name.length(); ++i) {
		if (cgi_name[i] == '-')
			cgi_name[i] = '_';
		else
			cgi_name[i] = std::toupper(cgi_name[i]);
	}
	_env_vars[cgi_name] = value;
}

void	CGIexecutor::setEnvKey(const std::string &key, const std::string &value) {
	_env_vars[key] = value;
}

void	CGIexecutor::setEnvKeySafe(const std::string &key, const std::string &value) {
	if (_env_vars.find(key) == _env_vars.end()) {
		_env_vars[key] = value;
	}
}

void	CGIexecutor::setupEnvironment() {
	_env_vars["GATEWAY_INTERFACE"] = "CGI/1.1";
	_env_vars["SERVER_PROTOCOL"] = "HTTP/1.1";
	_env_vars["SERVER_SOFTWARE"] = "webserv/1.0";

	// Set QUERY_STRING from config
	_env_vars["QUERY_STRING"] = _query_string;

	// Set defaults only if not already set by setter methods
	// setEnvKey("SERVER_NAME", "localhost");
	setEnvKey("SERVER_NAME", _config.server_names.empty() ? "localhost" : _config.server_names[0]);
	// setEnvKey("SERVER_PORT", "8080");
	setEnvKey("SERVER_PORT", _config.port > 0? std::to_string(_config.port) : "8080");
	// setEnvKey("REQUEST_METHOD", "GET");
	// setEnvKey("REMOTE_ADDR", "127.0.0.1");
	// setEnvKey("REMOTE_HOST", "localhost");

	_env_vars["PATH_INFO"] = "";
	_env_vars["PATH_TRANSLATED"] = "";
	_env_vars["SCRIPT_NAME"] = _script_path[0] == '/' ? _script_path : "/" + _script_path;
	char abs_script_path[PATH_MAX];
	if (realpath(_script_path.c_str(), abs_script_path)) {
		_env_vars["SCRIPT_FILENAME"] = std::string(abs_script_path);
	} else {
		_env_vars["SCRIPT_FILENAME"] = _script_path;
	}

	_env_vars["AUTH_TYPE"] = "";
	_env_vars["REMOTE_IDENT"] = "";
	_env_vars["REMOTE_USER"] = "";

	_env_vars["REDIRECT_STATUS"] = "200";
}

void	CGIexecutor::runChild(int pipe_in[2], int pipe_out[2]) {
	if (dup2(pipe_in[0], STDIN_FILENO) == -1) {
		Logger::error("dup2() failed for stdin");
		_error_type = CGIError::PIPE_FAILED;
		closePipes(pipe_in, pipe_out);
		exit(1);
	}
	if (dup2(pipe_out[1], STDOUT_FILENO) == -1) {
		Logger::error("dup2() failed for stdout");
		_error_type = CGIError::PIPE_FAILED;
		closePipes(pipe_in, pipe_out);
		exit(1);
	}

	closePipes(pipe_in, pipe_out);

	std::vector<char*> envp;
	for (std::map<std::string, std::string>::iterator it = _env_vars.begin();
			it != _env_vars.end();
			++it) {
		std::string env_str = it->first + "=" + it->second;
		envp.push_back(strdup(env_str.c_str()));
	}
	envp.push_back(nullptr);

	// Determine interpreter based on extension
	const char* argv[3];
	std::string cgi_ext = _script_path.substr(_script_path.find_last_of('.'));
	if (cgi_ext == ".sh") {
		argv[0] = _script_path.c_str();
		argv[1] = nullptr;
		execve(argv[0], (char**)argv, envp.data());
	}
	for (auto &loc : _config.locations) {
		if (loc.cgi_ext.has_value() && loc.cgi_ext.value() == cgi_ext && loc.cgi_path.has_value()) {
			argv[0] = loc.cgi_path.value().c_str();
			argv[1] = _script_path.c_str();
			argv[2] = nullptr;
			execve(argv[0], (char**)argv, envp.data());
		}
	}

	// If execve returns, it failed
	Logger::error("execve() failed for " + _script_path);
	_error_type = CGIError::EXEC_FAILED;
	// Cleanup
	for (size_t i = 0; i < envp.size(); ++i) {
		free(envp[i]);
	}
	exit(1);
}

int	CGIexecutor::start() {

	int		pipe_in[2];
	int		pipe_out[2];

	if (pipe(pipe_in) == -1) {
		Logger::error("pipe() failed");
		_error_type = CGIError::PIPE_FAILED;
		return 1;
	}
	if (pipe(pipe_out) == -1) {
		Logger::error("pipe() failed");
		_error_type = CGIError::PIPE_FAILED;
		safeClose(pipe_in[0]);
		safeClose(pipe_in[1]);
		return 1;
	}

	setupEnvironment();

	_pipe_in_fd = pipe_in[1];
	_pipe_out_fd = pipe_out[0];
	_child_pid = fork();
	if (_child_pid == -1) {
		Logger::error("fork() failed");
		_error_type = CGIError::FORK_FAILED;
		closePipes(pipe_in, pipe_out);
		return 1;
	}

	if (_child_pid == 0)
		runChild(pipe_in, pipe_out);

	safeClose(pipe_in[0]);
	safeClose(pipe_out[1]);

	// Set output pipe to non-blocking mode
	int flags = fcntl(_pipe_out_fd, F_GETFL, 0);
	if (flags == -1) {
		Logger::error("fcntl() failed to get flags");
		_error_type = CGIError::PIPE_FAILED;
		return 1;
	}
	if (fcntl(_pipe_out_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
		Logger::error("fcntl() failed to set non-blocking mode");
		_error_type = CGIError::PIPE_FAILED;
		return 1;
	}

	// Write POST data if exists
	if (!_post_data.empty()) {
		if (loopingWrite(_pipe_in_fd, _post_data.c_str(), _post_data.length()) == -1) {
			//cleanup
			killChildProcess();
			return 1;
		}
	}
	safeClose(_pipe_in_fd);

	return 0;
};

int	CGIexecutor::readOutput() {
	char buffer[BUFFER_SIZE];
	ssize_t bytes_read = read(_pipe_out_fd, buffer, BUFFER_SIZE - 1);
	if (bytes_read > 0) {
		// if (_output_buffer.size() + bytes_read > _max_output_size) {
		if (_output_buffer.size() + bytes_read > _config.client_max_body_size) {
			Logger::error("CGI output exceeded maximum allowed size");
			_error_type = CGIError::OUTPUT_TOO_LARGE;
			return -1;
		}

		try {
			buffer[bytes_read] = '\0';
			_output_buffer.append(buffer, bytes_read);
		}
		catch (const std::bad_alloc &e) {
			Logger::error("Out of memory reading CGI output");
			_error_type = CGIError::OUTPUT_TOO_LARGE;
			killChildProcess();
			return -1;
		}
	}
	return bytes_read;
}

bool	CGIexecutor::checkTimeout() {
	// bool timed_out = (time(NULL) - _start_time >= _timeout_seconds);
	bool timed_out = (time(NULL) - _start_time >= _config.client_timeout);
	if (timed_out){
		_error_type = CGIError::TIMEOUT;
	}
	return timed_out;

}

int	CGIexecutor::isComplete() {
	if (_child_pid == -1)
		return -1;

	int status;
	pid_t result = waitpid(_child_pid, &status, WNOHANG);
	if (result == 0) {
		return 0; // Still running
	} else if (result == _child_pid) {
		if (WIFEXITED(status)) {
			_exit_status = WEXITSTATUS(status);
		} else if (WIFSIGNALED(status)) {
			_exit_status = 128 + WTERMSIG(status); // Signal exit code
		} else {
			_exit_status = -1; // Unknown exit status
		}
		return 1; // Process has completed
	} else {
		Logger::error("waitpid() failed");
		_error_type = CGIError::UNKNOWN_ERROR;
		return -1; // Error
	}
}

std::string	CGIexecutor::getOutput() const {
	return _output_buffer;
}

void	CGIexecutor::setComplete(bool complete) {
	_is_complete = complete;
}

int	CGIexecutor::getOutputFd() const {
	return _pipe_out_fd;
}

int	CGIexecutor::getExitStatus() const {
	return _exit_status;
}

void	CGIexecutor::killChildProcess() {
	if (_child_pid > 0) {
		if (::kill(_child_pid, SIGKILL) == -1 && errno != ESRCH) {
			Logger::error("kill() failed: " + std::string(strerror(errno)));
			_error_type = CGIError::UNKNOWN_ERROR;
		}
		waitpid(_child_pid, NULL, WNOHANG);
		_child_pid = -1;
	}
	safeClose(_pipe_out_fd);
	safeClose(_pipe_in_fd);
}

// Error handling
CGIError::Type	CGIexecutor::getErrorType() const {
	return _error_type;
}

bool	CGIexecutor::hasError() const {
	return _error_type != CGIError::NO_ERROR;
}

//-----------------------//
//       RUN CGI         //
//-----------------------//

// Wrapper function for simple CGI execution
CGIexecutor*	runCGI(const std::string &script_path,
				const std::string &query_string,
				const std::string &post_data,
				const ServerConfig &config)
{
	CGIconfig	cgi_config(script_path, query_string, post_data, config);
	CGIexecutor*	cgi = new CGIexecutor(cgi_config);

	if (cgi->start() != 0) {
		delete cgi;
		return nullptr;
	}

	return cgi;
}

// Overload: script + config only
CGIexecutor*	runCGI(const std::string &script_path, const ServerConfig &config) {
	return runCGI(script_path, "", "", config);
}

// Overload: script + query + config (skip post_data)
CGIexecutor*	runCGI(const std::string &script_path,
					const std::string &query_string,
					const ServerConfig &config) {
	return runCGI(script_path, query_string, "", config);
}
