#ifndef CGIEXECUTOR_HPP
# define CGIEXECUTOR_HPP

# include <iostream> // For debugging and error messages
# include <cstring> // For working with C-style strings
# include <cerrno> // For errno
# include <vector> // For environment variable array
# include <map> // For environment variables

# include <unistd.h> // For fork, pipe, dup2, execve
# include <sys/wait.h> // For waitpid
# include <signal.h> // For kill
# include <poll.h> // For poll, used for timeout handling
# include <fcntl.h> // For fcntl (non-blocking mode)
# include <ctime> // For timeout handling

class CGIexecutor {
	private:
		std::string							_script_path;
		std::string							_query_string;
		std::string							_post_data;
		std::map<std::string, std::string>	_env_vars;
		time_t								_timeout_seconds;

		static constexpr int	DEFAULT_TIMEOUT_S = 10;
		static constexpr size_t	BUFFER_SIZE = 4096;
		static constexpr int	POLL_INTERVAL_MS = 100;

		void	runChild(int pipe_in[2], int pipe_out[2]);
		void	setupEnvironment();
		void	setEnvKey(const std::string &key, const std::string &value);

	public:
		CGIexecutor(const std::string &path);
		~CGIexecutor();

		void	setTimeout(int seconds);
		void	setQuery(const std::string &query);
		void	setPostData(const std::string &data);
		void	setRequestMethod(const std::string &method);
		void	setRequestURI(const std::string &uri);
		void	setServerInfo(const std::string &name, const std::string &port);
		void	setRemoteAddr(const std::string &addr);
		void	setHttpHeader(const std::string &name, const std::string &value);
		void	setContentType(const std::string &type);
		int		execute();
};

/**
 * Runs a CGI script with the given parameters and returns the exit status.
 * @param script_path The path to the CGI script to execute.
 * @param query_string The query string to pass to the CGI script (optional).
 * @param post_data The POST data to pass to the CGI script (optional).
 * @param timeout The maximum time to allow the CGI script to run before killing it (optional
 * defaults to 10 seconds).
 *
 * Example usage:
 * runCGI("script.py");
 * runCGI("script.py", "name=John");
 * runCGI("script.py", "name=John", 30);
 * runCGI("script.py", 30);
 * runCGI("script.py", "name=John", "data=value");
 * runCGI("script.py", "name=John", "data=value", 30);
 *
 * @return The exit status of the CGI script, or -1 on error.
 */
int	runCGI(const std::string &script_path,
			const std::string &query_string = "",
			const std::string &post_data = "",
			int timeout = 10);

int	runCGI(const std::string &script_path,
			const std::string &query_string,
			int timeout);

int	runCGI(const std::string &script_path, int timeout);



#endif
