#include "cgiError.hpp"

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
			return "Not Found";
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

std::string	CGIError::getErrorPage(Type errorType) {
	int statusCode = getStatusCode(errorType);
	std::string statusMessage = getStatusMessage(errorType);
	return "<html><head><title>" + std::to_string(statusCode) + " " + statusMessage + "</title></head>"
			"<body><h1>" + std::to_string(statusCode) + " " + statusMessage + "</h1>"
			"<p>An error occurred while processing your request.</p></body></html>";
};
