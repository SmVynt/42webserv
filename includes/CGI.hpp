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

		void	runChild(int pipe_in[2], int pipe_out[2]);
		void	setQuery(const std::string &query);
		void	setPostData(const std::string &data);
		void	setupEnvironment();
		void	setTimeout(int seconds);

		static const int	DEFAULT_TIMEOUT = 10;
		static const int	BUFFER_SIZE = 4096;
		static const int	POLL_INTERVAL_MS = 100;
	public:
		CGIexecutor(const std::string &path);
		~CGIexecutor();

		int		execute();
};

#endif
