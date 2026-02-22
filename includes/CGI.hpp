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
// # include <exception>
# include "cgiUtils.hpp"
# include "cgiError.hpp"
# include "Logger.hpp"
# include "Cluster.hpp"
# include "Config.hpp"

class CGIconfig {
	public:
		std::string			script_path;
		std::string			query_string;
		std::string			post_data;
		// int				timeout;
		// unsigned long	max_output_size;
		const ServerConfig	&_config;

		CGIconfig(const std::string &path,
				  const std::string &query,
				  const std::string &post,
				  const ServerConfig &config);

		~CGIconfig();
};

class CGIexecutor {
	private:
		std::string							_script_path;
		std::string							_query_string;
		std::string							_post_data;
		std::map<std::string, std::string>	_env_vars;

		// time_t								_timeout_seconds;
		// unsigned long						_max_output_size;
		const ServerConfig					&_config;
		time_t								_start_time;
		pid_t								_child_pid;
		int									_pipe_out_fd;
		int									_pipe_in_fd;
		int									_exit_status;
		std::string							_output_buffer;
		bool 								_is_complete;
		CGIError::Type						_error_type;

		// static constexpr int	DEFAULT_TIMEOUT_S = 10;
		static constexpr size_t	BUFFER_SIZE = 4096;
		static constexpr size_t	DEFAULT_MAX_OUTPUT_SIZE = 10 * 1024 * 1024;
		static constexpr int	POLL_INTERVAL_MS = 100;

		void	runChild(int pipe_in[2], int pipe_out[2]);
		void	setupEnvironment();
		void	setEnvKey(const std::string &key, const std::string &value);

	public:
		CGIexecutor(const CGIconfig &config);
		~CGIexecutor();

		// void	setTimeout(int seconds);
		void	setQuery(const std::string &query);
		void	setPostData(const std::string &data);
		void	setHttpHeader(const std::string &name, const std::string &value);

		int			start();
		void		setComplete(bool complete);
		int			isComplete();
		int			readOutput();
		bool		checkTimeout();
		int			getOutputFd() const;
		int			getExitStatus() const;
		std::string	getOutput() const;
		void		killChildProcess();

		CGIError::Type	getErrorType() const;
		bool			hasError() const;
};

/**
 * Runs a CGI script with the given parameters and returns the exit status.
 * @param script_path The path to the CGI script to execute.
 * @param query_string The query string to pass to the CGI script (optional).
 * @param post_data The POST data to pass to the CGI script (optional).
 * @param config Server configuration containing timeout and max body size settings.
 *
 * Example usage:
 * runCGI("script.py");
 * runCGI("script.py", "name=John");
 * runCGI("script.py", 30);
 * runCGI("script.py", "name=John", "data=value");
 * runCGI("script.py", "name=John", "data=value", 30);
 *
 * @return The exit status of the CGI script, or -1 on error.
 */
CGIexecutor*	runCGI(const std::string &script_path,
				const std::string &query_string,
				const std::string &post_data,
				const ServerConfig &config);

CGIexecutor*	runCGI(const std::string &script_path,
				const std::string &query_string,
				const ServerConfig &config);

CGIexecutor*	runCGI(const std::string &script_path,
				const ServerConfig &config);

#endif
