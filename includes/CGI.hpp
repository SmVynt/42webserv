#pragma once

#include "hub.hpp"

#include "cgiError.hpp"
#include "Config.hpp"
#include "cgiUtils.hpp"
#include "Logger.hpp"

/**
 * @brief Immutable input bundle used to construct a CGI executor.
 */
class CGIconfig {
	public:
		std::string			script_path;
		std::string			request_uri;
		std::string			query_string;
		const ServerConfig	&_config;

		/**
		 * @brief Builds a CGI configuration context for one request.
		 */
		CGIconfig(const std::string &path,
				  const std::string &uri,
				  const std::string &query,
				  const ServerConfig &config);
		/**
		 * @brief Destroys the configuration wrapper.
		 */
		~CGIconfig();
};

/**
 * @brief Launches and manages one CGI child process lifecycle.
 */
class CGIexecutor {
	public:
		/**
		 * @brief Creates an executor from parsed CGI request data.
		 */
		CGIexecutor(const CGIconfig &config);

		/**
		 * @brief Ensures child process and owned FDs are cleaned up.
		 */
		~CGIexecutor();

		void			setQuery(const std::string &query);
		void			setPostDataSize(size_t data_size);
		void			setHttpHeader(const std::string &name, const std::string &value);
		void			setEnvKey(const std::string &key, const std::string &value);
		void			setEnvKeySafe(const std::string &key, const std::string &value);
		CGIError::Type	getErrorType() const;
		bool			hasError() const;
		int				getOutputFd() const;
		int				getInputFd() const;
		int				getExitStatus() const;
		std::string		getOutput() const;
		void			setComplete(bool complete);

		/**
		 * @brief Creates pipes, forks, and starts CGI execution.
		 * @return 0 on success, non-zero on startup failure.
		 */
		int				start();

		/**
		 * @brief Polls child status using non-blocking waitpid().
		 * @return 1 finished, 0 running, -1 on error.
		 */
		int				isComplete();

		/**
		 * @brief Terminates CGI child and closes owned pipe FDs.
		 */
		void			killChildProcess();

		/**
		 * @brief Detaches a pipe FD from executor ownership.
		 */
		void			detachPipeFd(int fd);

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

		/**
		 * @brief Validates script availability/executability in child.
		 */
		void		childCheckCgiPath();

		/**
		 * @brief Wires CGI stdin/stdout to pipes in child process.
		 */
		void		childResolvePipes(int pipe_in[2], int pipe_out[2]);

		/**
		 * @brief Resolves interpreter path after child chdir() logic.
		 */
		std::string	childResolvedInterpreterPath(const std::string& config_cgi_path,
						const std::string& script_path);

		/**
		 * @brief Child-side execution path ending in execve().
		 */
		void		runChild(int pipe_in[2], int pipe_out[2]);

		/**
		 * @brief Populates CGI environment variables from request/config.
		 */
		void		setupEnvironment();
};
