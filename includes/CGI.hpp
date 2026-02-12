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

class CGIconfig {
	public:
		std::string	script_path;
		std::string	query_string;
		std::string	post_data;
		int			timeout;

		CGIconfig(const std::string &path,
				  const std::string &query = "",
				  const std::string &post = "",
				  int timeout_sec = 10)
			: script_path(path), query_string(query), post_data(post), timeout(timeout_sec) {};

		~CGIconfig() {};
};

class CGIexecutor {
	private:
		std::string							_script_path;
		std::string							_query_string;
		std::string							_post_data;
		std::map<std::string, std::string>	_env_vars;

		time_t								_timeout_seconds;
		time_t								_start_time;
		pid_t								_child_pid;
		int									_pipe_out_fd;
		int									_pipe_in_fd;
		int									_exit_status;
		std::string							_output_buffer;
		bool 								_is_complete;

		static constexpr int	DEFAULT_TIMEOUT_S = 10;
		static constexpr size_t	BUFFER_SIZE = 4096;
		static constexpr int	POLL_INTERVAL_MS = 100;

		void	runChild(int pipe_in[2], int pipe_out[2]);
		void	setupEnvironment();
		void	setEnvKey(const std::string &key, const std::string &value);

	public:
		CGIexecutor(const CGIconfig &config);
		~CGIexecutor();

		void	setTimeout(int seconds);
		void	setQuery(const std::string &query);
		void	setPostData(const std::string &data);
		void	setHttpHeader(const std::string &name, const std::string &value);

		int			start();
		void		setComplete(bool complete);
		bool		isComplete();
		bool		readOutput();
		bool		checkTimeout() const;
		int			getOutputFd() const;
		int			getExitStatus() const;
		std::string	getOutput() const;
		void		killChildProcess();
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
CGIexecutor*	runCGI(const std::string &script_path,
				const std::string &query_string = "",
				const std::string &post_data = "",
				int timeout = 10);

CGIexecutor*	runCGI(const std::string &script_path,
				const std::string &query_string,
				int timeout);

CGIexecutor*	runCGI(const std::string &script_path, int timeout);

#endif
