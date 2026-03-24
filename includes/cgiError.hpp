#ifndef CGIERROR_HPP
# define CGIERROR_HPP

# include <string>

class CGIError{
	public:
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

		static int			getStatusCode(Type errorType);
		static std::string	getStatusMessage(Type errorType);
		// static std::string	getErrorPage(Type errorType);
		static Type			getErrorFromExit(int exitCode);
		static int 			getExitFromError(Type errorType);
};

#endif
