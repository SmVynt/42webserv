#include "CGI.hpp"

CGIexecutor::CGIexecutor(const std::string &path)
	: _script_path(path), _timeout_seconds(DEFAULT_TIMEOUT_S) {}

CGIexecutor::~CGIexecutor() {}

void	CGIexecutor::setEnvKey(const std::string &key, const std::string &value) {
	if (_env_vars.find(key) == _env_vars.end())
		_env_vars[key] = value;
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

void	CGIexecutor::setRequestMethod(const std::string &method) {
	_env_vars["REQUEST_METHOD"] = method;
}

void	CGIexecutor::setRequestURI(const std::string &uri) {
	_env_vars["REQUEST_URI"] = uri;
}

void	CGIexecutor::setServerInfo(const std::string &name, const std::string &port) {
	_env_vars["SERVER_NAME"] = name;
	_env_vars["SERVER_PORT"] = port;
}

void	CGIexecutor::setRemoteAddr(const std::string &addr) {
	_env_vars["REMOTE_ADDR"] = addr;
	_env_vars["REMOTE_HOST"] = addr;
}

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

void	CGIexecutor::setContentType(const std::string &type) {
	_env_vars["CONTENT_TYPE"] = type;
}

void	CGIexecutor::setupEnvironment() {
	_env_vars["GATEWAY_INTERFACE"] = "CGI/1.1";
	_env_vars["SERVER_PROTOCOL"] = "HTTP/1.1";
	_env_vars["SERVER_SOFTWARE"] = "webserv/1.0";

	// Set defaults only if not already set by setter methods
	setEnvKey("SERVER_NAME", "localhost");
	setEnvKey("SERVER_PORT", "8080");
	setEnvKey("REQUEST_METHOD", "GET");
	setEnvKey("QUERY_STRING", "");
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
	dup2(pipe_in[0], STDIN_FILENO);
	dup2(pipe_out[1], STDOUT_FILENO);

	close(pipe_in[0]);
	close(pipe_in[1]);
	close(pipe_out[0]);
	close(pipe_out[1]);

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
	exit(1);
}

int	CGIexecutor::execute() {


	int		pipe_in[2];
	int		pipe_out[2];
	pid_t	pid;

	if (pipe(pipe_in) == -1 || pipe(pipe_out) == -1) {
		std::cerr << "Error: pipe() failed" << std::endl;
		return 1;
	}
	setupEnvironment();
	pid = fork();
	if (pid == -1) {
		std::cerr << "Error: fork() failed" << std::endl;
		return 1;
	}
	if (pid == 0)
		runChild(pipe_in, pipe_out);

	close(pipe_in[0]);
	close(pipe_out[1]);

	int	flags = fcntl(pipe_out[0], F_GETFL, 0);
	fcntl(pipe_out[0], F_SETFL, flags | O_NONBLOCK);

	if (!_post_data.empty()) {
		write(pipe_in[1], _post_data.c_str(), _post_data.length());
	}
	close(pipe_in[1]);

	std::cout << "=== CGI Output ===" << std::endl;
	char			buffer[BUFFER_SIZE];
	time_t			start_time = time(NULL);
	int				status = 0;

	struct pollfd	poll_fd;
	poll_fd.fd = pipe_out[0];
	poll_fd.events = POLLIN;

	// Read output as it becomes available to prevent pipe buffer from filling
	while (true) {
		// Check if child process has exited
		if (waitpid(pid, &status, WNOHANG) == pid)
			break;

		// Check timeout
		if (time(NULL) - start_time >= _timeout_seconds) {
			std::cerr << "Error: CGI timeout" << std::endl;
			kill(pid, SIGKILL);
			waitpid(pid, NULL, 0);
			close(pipe_out[0]);
			std::cout << "\n=== CGI Exit Code: 504 (Gateway Timeout) ===" << std::endl;
			return 504;
		}

		// Drain the pipe
		int poll_result = poll(&poll_fd, 1, POLL_INTERVAL_MS);
		if (poll_result > 0 && (poll_fd.revents & POLLIN)) {
			ssize_t	bytes_read;
			while ((bytes_read = read(pipe_out[0], buffer, BUFFER_SIZE - 1)) > 0) {
				buffer[bytes_read] = '\0';
				std::cout << buffer;
	}
		}
	}

	// Read any remaining data after child exits
	ssize_t	bytes_read;
	while ((bytes_read = read(pipe_out[0], buffer, BUFFER_SIZE - 1)) > 0) {
		buffer[bytes_read] = '\0';
		std::cout << buffer;
	}

	close(pipe_out[0]);

	waitpid(pid, &status, 0);

	if (WIFEXITED(status)) {
		int exit_code = WEXITSTATUS(status);
		std::cout << "\n=== CGI Exit Code: " << exit_code << " ===" << std::endl;
		return exit_code;
	} else if (WIFSIGNALED(status)) {
		std::cout << "\n=== CGI Killed by signal ===" << std::endl;
		return 500;
	}

	return 1;
};

// Wrapper function for simple CGI execution
int	runCGI(const std::string &script_path,
		   const std::string &query_string,
		   const std::string &post_data,
		   int timeout)
{
	CGIexecutor cgi(script_path);
	cgi.setTimeout(timeout);

	if (!query_string.empty())
		cgi.setQuery(query_string);

	if (!post_data.empty())
		cgi.setPostData(post_data);

	return cgi.execute();
}

// Overload: script + timeout only
int	runCGI(const std::string &script_path, int timeout)
{
	return runCGI(script_path, "", "", timeout);
}

// Overload: script + query + timeout (skip post_data)
int	runCGI(const std::string &script_path,
		   const std::string &query_string,
		   int timeout)
{
	return runCGI(script_path, query_string, "", timeout);
}
