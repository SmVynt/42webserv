#include "cgiError.hpp"

// Maps internal CGI executor failures to the HTTP status sent to the client.
int CGIError::getStatusCode(Type errorType) {
	switch (errorType) {
		case NO_ERROR:
			return 0;
		case SCRIPT_NOT_FOUND:
			return 404;
		case SCRIPT_NOT_EXECUTABLE:
		case FORK_FAILED:
		case EXEC_FAILED:
		case PIPE_FAILED:
		case OUTPUT_TOO_LARGE:
			return 500;
		case TIMEOUT:
			return 504;
		case MALFORMED_RESPONSE:
			return 502;
		default:
			return 500;
	}
};

std::string	CGIError::getStatusMessage(Type errorType) {
	switch (errorType) {
		case NO_ERROR:
			return "OK";
		case SCRIPT_NOT_FOUND:
			return "Script not Found";
		case SCRIPT_NOT_EXECUTABLE:
			return "Internal Server Error";
		case TIMEOUT:
			return "Gateway Timeout";
		case FORK_FAILED:
		case EXEC_FAILED:
		case PIPE_FAILED:
		case OUTPUT_TOO_LARGE:
			return "Internal Server Error";
		case MALFORMED_RESPONSE:
			return "Bad Gateway";
		default:
			return "Internal Server Error";
	}
};

CGIError::Type	CGIError::getErrorFromExit(int exitCode) {
	// 52..59 are project-specific CGI child exit codes (see getExitFromError).
	switch (exitCode) {
		case 0:
			return NO_ERROR;
		case 52:
			return SCRIPT_NOT_FOUND;
		case 53:
			return SCRIPT_NOT_EXECUTABLE;
		case 54:
			return FORK_FAILED;
		case 55:
			return EXEC_FAILED;
		case 56:
			return PIPE_FAILED;
		case 57:
			return OUTPUT_TOO_LARGE;
		case 58:
			return TIMEOUT;
		case 59:
			return MALFORMED_RESPONSE;
		default:
			return UNKNOWN_ERROR;
	}
};
int 			CGIError::getExitFromError(Type errorType) {
	// Keep this mapping in sync with getErrorFromExit().
	switch (errorType) {
		case NO_ERROR:
			return 0;
		case SCRIPT_NOT_FOUND:
			return 52;
		case SCRIPT_NOT_EXECUTABLE:
			return 53;
		case FORK_FAILED:
			return 54;
		case EXEC_FAILED:
			return 55;
		case PIPE_FAILED:
			return 56;
		case OUTPUT_TOO_LARGE:
			return 57;
		case TIMEOUT:
			return 58;
		case MALFORMED_RESPONSE:
			return 59;
		default:
			return 1;
	}
};
