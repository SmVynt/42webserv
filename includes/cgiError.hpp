#ifndef CGIERROR_HPP
# define CGIERROR_HPP

# include <string>

/**
 * @brief Maps CGI internal failures to HTTP status and process exit codes.
 */
class CGIError{
	public:
		/**
		 * @brief Canonical CGI error categories used across executor and cluster.
		 */
		enum Type {
			NO_ERROR,
			SCRIPT_NOT_FOUND,		// 404
			SCRIPT_NOT_EXECUTABLE,	// 500
			TIMEOUT,				// 504
			FORK_FAILED,			// 500
			EXEC_FAILED,			// 500
			PIPE_FAILED,			// 500
			OUTPUT_TOO_LARGE,		// 500
			MALFORMED_RESPONSE,		// 502
			UNKNOWN_ERROR
		};

		/**
		 * @brief Converts CGI error type to HTTP status code.
		 */
		static int			getStatusCode(Type errorType);
		
		/**
		 * @brief Returns a human-readable message for a CGI error type.
		 */
		static std::string	getStatusMessage(Type errorType);

		/**
		 * @brief Converts child process exit code to CGI error type.
		 */
		static Type			getErrorFromExit(int exitCode);

		/**
		 * @brief Converts CGI error type to child process exit code.
		 */
		static int 			getExitFromError(Type errorType);
};

#endif
