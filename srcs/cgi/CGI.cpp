#include "CGI.hpp"

//****************************************************//
//*********** CGIconfig Class Functions **************//
//****************************************************//

CGIconfig::CGIconfig(const std::string &path,
					const std::string &uri,
					const std::string &query,
					const ServerConfig &config) :
	script_path(path),
	request_uri(uri),
	query_string(query),
	_config(config)
	{}

CGIconfig::~CGIconfig() {}

//****************************************************//
//*********** CGIexecutor Class Functions ************//
//****************************************************//

CGIexecutor::CGIexecutor(const CGIconfig &CGIconfig) :
	_script_path(CGIconfig.script_path),
	_request_uri(CGIconfig.request_uri),
	_query_string(CGIconfig.query_string),
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
	Logger::debug("CGIexecutor destroyed for script: " + _script_path);
}

//****************************************************//
//*********** Setter Functions ***********************//
//****************************************************//

void	CGIexecutor::setQuery(const std::string &query)
{
	_query_string = query;
	_env_vars["QUERY_STRING"] = query;
}

void	CGIexecutor::setPostDataSize(size_t data_size) {
	_env_vars["CONTENT_LENGTH"] = std::to_string(data_size);
	_env_vars["REQUEST_METHOD"] = "POST";
}

void	CGIexecutor::setHttpHeader(const std::string &name, const std::string &value) {
	// HTTP headers become HTTP_<NAME> with dashes as underscores, uppercased.
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
	// Unlike setEnvKey, this won't overwrite a value already set by the request headers.
	if (_env_vars.find(key) == _env_vars.end()) {
		_env_vars[key] = value;
	}
}

void	CGIexecutor::setupEnvironment() {
	// Set mandatory environment variables
	_env_vars["GATEWAY_INTERFACE"] = "CGI/1.1";
	_env_vars["SERVER_PROTOCOL"] = "HTTP/1.1";
	_env_vars["SERVER_SOFTWARE"] = "webserv/1.0";

	_env_vars["QUERY_STRING"] = _query_string;

	setEnvKey("SERVER_NAME", _config.server_names.empty() ? "localhost" : _config.server_names[0]);
	setEnvKey("SERVER_PORT", _config.port > 0? std::to_string(_config.port) : "8080");
	_env_vars["REQUEST_URI"] = _request_uri;
	_env_vars["PATH_INFO"] = _request_uri;
	_env_vars["PATH_TRANSLATED"] = _script_path;
	_env_vars["SCRIPT_NAME"] = "";

	_env_vars["SCRIPT_FILENAME"] = _script_path;

	_env_vars["AUTH_TYPE"] = "";
	_env_vars["REMOTE_IDENT"] = "";
	_env_vars["REMOTE_USER"] = "";

	_env_vars["REDIRECT_STATUS"] = "200";
}

//****************************************************//
//*********** Child Process Functions ****************//
//****************************************************//

void	CGIexecutor::childCheckCgiPath() {
	// POST skipped: the 42 tester expects only the interpreter to be executable for POST,
	// not the script itself (the interpreter receives the script as an argument).
	if (_env_vars["REQUEST_METHOD"] != "POST") {
		if (access(_script_path.c_str(), F_OK) != 0) {
			Logger::error("CGI script not found: " + _script_path);
			exit(CGIError::getExitFromError(CGIError::SCRIPT_NOT_FOUND));
		}
		if (access(_script_path.c_str(), X_OK) != 0) {
			Logger::error("CGI script not executable: " + _script_path);
			exit(CGIError::getExitFromError(CGIError::SCRIPT_NOT_EXECUTABLE));
		}
	}
}

void	CGIexecutor::childResolvePipes(int pipe_in[2], int pipe_out[2]) {
	// Duplicate the input pipe to stdin
	if (dup2(pipe_in[0], STDIN_FILENO) == -1) {
		Logger::error("dup2() failed for stdin");
		exit(CGIError::getExitFromError(CGIError::PIPE_FAILED));
	}
	if (dup2(pipe_out[1], STDOUT_FILENO) == -1) {
		Logger::error("dup2() failed for stdout");
		exit(CGIError::getExitFromError(CGIError::PIPE_FAILED));
	}

	closePipes(pipe_in, pipe_out);
}

std::string	CGIexecutor::childResolvedInterpreterPath(const std::string& config_cgi_path,
									const std::string& script_path)
{
	// If the config CGI path is empty or starts with a slash, return it
	if (config_cgi_path.empty() || config_cgi_path[0] == '/')
		return config_cgi_path;
	size_t slash = script_path.find_last_of('/');
	if (slash == std::string::npos)
		return config_cgi_path;

	std::string tail = config_cgi_path;
	// If the tail starts with ./, remove the ./
	if (tail.length() >= 2 && tail[0] == '.' && tail[1] == '/')
		tail = tail.substr(2);
	// Because of chdir(), we need to go up one level to reach the interpreter
	return (std::string("../") + tail);
}

void	CGIexecutor::runChild(int pipe_in[2], int pipe_out[2]) {
	childCheckCgiPath();
	childResolvePipes(pipe_in, pipe_out);

	// Get the script name from the script path
	std::string script_name = _script_path;
	size_t last_slash = _script_path.find_last_of('/');
	if (last_slash != std::string::npos) {
		std::string dir = _script_path.substr(0, last_slash);
		script_name = _script_path.substr(last_slash + 1);
		if (!dir.empty())
			chdir(dir.c_str());
	}

	// Create the environment variables
	std::vector<char*> envp;
	for (std::map<std::string, std::string>::iterator it = _env_vars.begin();
			it != _env_vars.end();
			++it) {
		std::string env_str = it->first + "=" + it->second;
		envp.push_back(strdup(env_str.c_str()));
	}
	envp.push_back(nullptr);

	// Get the CGI extension
	const char* argv[3];
	std::string cgi_ext = script_name.substr(script_name.find_last_of('.'));
	if (cgi_ext == ".sh") {
		// Shell scripts are self-executable via their shebang; no separate interpreter needed.
		std::string local_path = "./" + script_name;
		argv[0] = local_path.c_str();
		argv[1] = nullptr;
		execve(argv[0], (char**)argv, envp.data());
	}
	// If the CGI extension is not .sh, try to find the interpreter in the locations
	std::string resolved_path;
	for (const auto &loc : _config.locations) {
		if (loc.cgi_ext.has_value() && loc.cgi_ext.value() == cgi_ext && loc.cgi_path.has_value()) {
			// Resolve the interpreter path
			resolved_path = childResolvedInterpreterPath(loc.cgi_path.value(), _script_path);
			argv[0] = resolved_path.c_str();
			argv[1] = script_name.c_str();
			argv[2] = nullptr;
			execve(argv[0], (char**)argv, envp.data());
		}
	}

	// If execve returns, it failed
	Logger::error("execve() failed for " + _script_path);
	// Cleanup the environment variables
	for (size_t i = 0; i < envp.size(); ++i) {
		free(envp[i]);
	}
	exit(1);
}

//****************************************************//
//*********** Main Process Functions *****************//
//****************************************************//

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

	// Close the child-side ends that the parent doesn't use.
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

	// Set input pipe to non-blocking mode (write end)
	int in_flags = fcntl(_pipe_in_fd, F_GETFL, 0);
	if (in_flags == -1) {
		Logger::error("fcntl() failed to get flags for pipe_in");
		_error_type = CGIError::PIPE_FAILED;
		return 1;
	}
	if (fcntl(_pipe_in_fd, F_SETFL, in_flags | O_NONBLOCK) == -1) {
		Logger::error("fcntl() failed to set non-blocking mode for pipe_in");
		_error_type = CGIError::PIPE_FAILED;
		return 1;
	}

	return 0;
};

int	CGIexecutor::isComplete() {
	// Non-blocking check: returns 0 (still running), 1 (exited), or -1 (error/no child).
	if (_child_pid == -1)
		return -1;

	int status;
	pid_t result = waitpid(_child_pid, &status, WNOHANG);
	if (result == 0) {
		return 0; // Still running
	} else if (result == _child_pid) {
		Logger::debug("Exited with code: " + std::to_string(WEXITSTATUS(status)));
		_error_type = CGIError::getErrorFromExit(WEXITSTATUS(status));
		Logger::debug("Mapped error type: " + std::to_string(CGIError::getStatusCode(_error_type)));
		if (WIFEXITED(status)) {
			_exit_status = WEXITSTATUS(status);
		} else if (WIFSIGNALED(status)) {
			_exit_status = 128 + WTERMSIG(status); // Shell convention: signal N → exit 128+N
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

int	CGIexecutor::getInputFd() const {
	return _pipe_in_fd;
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

CGIError::Type	CGIexecutor::getErrorType() const {
	return _error_type;
}

bool	CGIexecutor::hasError() const {
	return _error_type != CGIError::NO_ERROR;
}

// Called by Cluster after it closes a pipe FD itself, so the destructor won't double-close it.
void	CGIexecutor::detachPipeFd(int fd) {
	if (_pipe_in_fd == fd)
		_pipe_in_fd = -1;
	if (_pipe_out_fd == fd)
		_pipe_out_fd = -1;
}
