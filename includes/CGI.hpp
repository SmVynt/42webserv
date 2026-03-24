#ifndef CGI_HPP
# define CGI_HPP

# include <iostream>
# include <cstring>
# include <cerrno>
# include <vector>
# include <map>
# include <unistd.h>
# include <sys/wait.h> 
# include <signal.h>
# include <poll.h>
# include <fcntl.h>
# include <ctime>

# include "cgiUtils.hpp"
# include "cgiError.hpp"
# include "Logger.hpp"
# include "Config.hpp"

// CGIconfig class for storing CGI configuration
class CGIconfig {
	public:
		std::string			script_path;
		std::string			request_uri;
		std::string			query_string;
		const ServerConfig	&_config;

		CGIconfig(const std::string &path,
				  const std::string &uri,
				  const std::string &query,
				  const ServerConfig &config);
		~CGIconfig();
};

// CGIexecutor class for executing CGI scripts
class CGIexecutor {
	public:
		CGIexecutor(const CGIconfig &config);
		~CGIexecutor();

		void			setQuery(const std::string &query);
		void			setPostDataSize(size_t data_size);
		void			setHttpHeader(const std::string &name, const std::string &value);
		void			setEnvKey(const std::string &key, const std::string &value);
		void			setEnvKeySafe(const std::string &key, const std::string &value);

		int				start();
		void			setComplete(bool complete);
		int				isComplete();
		int				getOutputFd() const;
		int				getInputFd() const;
		int				getExitStatus() const;
		std::string		getOutput() const;
		void			killChildProcess();
		void			detachPipeFd(int fd);

		CGIError::Type	getErrorType() const;
		bool			hasError() const;
		
	private:
		std::string							_script_path;
		std::string							_request_uri;
		std::string							_query_string;
		std::map<std::string, std::string>	_env_vars;

		const ServerConfig					&_config;
		time_t								_start_time;
		pid_t								_child_pid;
		int									_pipe_out_fd;
		int									_pipe_in_fd;
		int									_exit_status;
		std::string							_output_buffer;
		bool 								_is_complete;
		CGIError::Type						_error_type;

		void		childCheckCgiPath();
		void		childResolvePipes(int pipe_in[2], int pipe_out[2]);
		std::string	childResolvedInterpreterPath(const std::string& config_cgi_path,
						const std::string& script_path);
		void		runChild(int pipe_in[2], int pipe_out[2]);
		void		setupEnvironment();
};

#endif
