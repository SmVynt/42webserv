#include "CGI.hpp"

CGIconfig::CGIconfig(const std::string &path,
				   const std::string &query,
				   const std::string &post,
				   int timeout_sec) :
	script_path(path), query_string(query), post_data(post), timeout(timeout_sec) {}

CGIconfig::~CGIconfig() {}

CGIexecutor::CGIexecutor(const CGIconfig &config) :
	_script_path(config.script_path),
	_query_string(config.query_string),
	_post_data(config.post_data),
	_timeout_seconds(config.timeout) {
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

void	CGIexecutor::setTimeout(int seconds) {
	_timeout_seconds = seconds;
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

// void	CGIexecutor::setContentType(const std::string &type) {
// 	_env_vars["CONTENT_TYPE"] = type;
// }

void	CGIexecutor::setEnvKey(const std::string &key, const std::string &value) {
	if (_env_vars.find(key) == _env_vars.end())
		_env_vars[key] = value;
}

void	CGIexecutor::setupEnvironment() {
	_env_vars["GATEWAY_INTERFACE"] = "CGI/1.1";
	_env_vars["SERVER_PROTOCOL"] = "HTTP/1.1";
	_env_vars["SERVER_SOFTWARE"] = "webserv/1.0";

	// Set QUERY_STRING from config
	_env_vars["QUERY_STRING"] = _query_string;

	// Set defaults only if not already set by setter methods
	setEnvKey("SERVER_NAME", "localhost");
	setEnvKey("SERVER_PORT", "8080");
	setEnvKey("REQUEST_METHOD", "GET");
	setEnvKey("REMOTE_ADDR", "127.0.0.1");
	setEnvKey("REMOTE_HOST", "localhost");

	_env_vars["PATH_INFO"] = "";
	_env_vars["PATH_TRANSLATED"] = "";
	_env_vars["SCRIPT_NAME"] = "/cgi-bin/test";
	_env_vars["SCRIPT_FILENAME"] = _script_path;

	_env_vars["AUTH_TYPE"] = "";
	_env_vars["REMOTE_IDENT"] = "";
	_env_vars["REMOTE_USER"] = "";

	_env_vars["REDIRECT_STATUS"] = "200";
}

void	CGIexecutor::runChild(int pipe_in[2], int pipe_out[2]) {
	if (dup2(pipe_in[0], STDIN_FILENO) == -1) {
		std::cerr << "Error: dup2() failed for stdin" << std::endl;
		closePipes(pipe_in, pipe_out);
		exit(1);
	}
	if (dup2(pipe_out[1], STDOUT_FILENO) == -1) {
		std::cerr << "Error: dup2() failed for stdout" << std::endl;
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
	if (_script_path.find(".py") != std::string::npos) {
		argv[0] = "/usr/bin/python3";
		argv[1] = _script_path.c_str();
		argv[2] = nullptr;
		execve(argv[0], (char**)argv, envp.data());
	} else if (_script_path.find(".php") != std::string::npos) {
		argv[0] = "/usr/bin/php-cgi";
		argv[1] = _script_path.c_str();
		argv[2] = nullptr;
		execve(argv[0], (char**)argv, envp.data());
	} else {
		argv[0] = _script_path.c_str();
		argv[1] = nullptr;
		execve(argv[0], (char**)argv, envp.data());
	}

	// If execve returns, it failed
	std::cerr << "Error: execve() failed for " << _script_path << std::endl;
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
		std::cerr << "Error: pipe() failed" << std::endl;
		return 1;
	}
	if (pipe(pipe_out) == -1) {
		std::cerr << "Error: pipe() failed" << std::endl;
		safeClose(pipe_in[0]);
		safeClose(pipe_in[1]);
		return 1;
	}

	setupEnvironment();

	_pipe_in_fd = pipe_in[1];
	_pipe_out_fd = pipe_out[0];
	_child_pid = fork();
	if (_child_pid == -1) {
		std::cerr << "Error: fork() failed" << std::endl;
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
		std::cerr << "Error: fcntl() failed to get flags" << std::endl;
		return 1;
	}
	if (fcntl(_pipe_out_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
		std::cerr << "Error: fcntl() failed to set non-blocking mode" << std::endl;
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
		buffer[bytes_read] = '\0';
		_output_buffer += buffer;
		// std::cout << buffer;
		// return true;
	}
	return bytes_read;
}

bool	CGIexecutor::checkTimeout() const {
	return (time(NULL) - _start_time >= _timeout_seconds);
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
		std::cerr << "Error: waitpid() failed" << std::endl;
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
			std::cerr << "Error: kill() failed: " << strerror(errno) << std::endl;
		}
		waitpid(_child_pid, NULL, WNOHANG);
		_child_pid = -1;
	}
	safeClose(_pipe_out_fd);
	safeClose(_pipe_in_fd);
}

// Wrapper function for simple CGI execution
CGIexecutor*	runCGI(const std::string &script_path,
				const std::string &query_string,
				const std::string &post_data,
				int timeout)
{
	CGIconfig	config(script_path, query_string, post_data, timeout);
	CGIexecutor*	cgi = new CGIexecutor(config);

	if (cgi->start() != 0) {
		delete cgi;
		return nullptr;
	}

	return cgi;
}

// Overload: script + query + post (use default timeout)
CGIexecutor*	runCGI(const std::string &script_path,
					const std::string &query_string,
					const std::string &post_data) {
	return runCGI(script_path, query_string, post_data, 10);
}

// Overload: script + timeout only
CGIexecutor*	runCGI(const std::string &script_path, int timeout) {
	return runCGI(script_path, "", "", timeout);
}

// Overload: script + query + timeout (skip post_data)
CGIexecutor*	runCGI(const std::string &script_path,
					const std::string &query_string,
					int timeout) {
	return runCGI(script_path, query_string, "", timeout);
}

// Overload: script + query only (use default timeout)
CGIexecutor*	runCGI(const std::string &script_path,
					const std::string &query_string) {
	return runCGI(script_path, query_string, "", 10);
}

// Overload: script only (use defaults)
CGIexecutor*	runCGI(const std::string &script_path) {
	return runCGI(script_path, "", "", 10);
}
