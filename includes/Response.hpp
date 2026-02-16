#ifndef RESPONSE_HPP
# define RESPONSE_HPP

# include "Request.hpp"
# include "Config.hpp"

#include <sys/types.h>
#include <sys/socket.h>

class Response {
private:
	std::string	_version;
	int			_status_code;
	std::string	_body;
	std::map<std::string, std::string>	_headers;

	std::string	_full_response;
	size_t		_bytes_sent;

	const static std::map<int, std::string>	_status_messages;
	static std::map<int, std::string>	_initStatusMessages();

public:
	Response();
	~Response();
	void setStatusCode(int code);
	void setBody(const std::string &body);
	void addHeader(const std::string &key, const std::string &value);
	std::string build();

	void	prepare();
	int		sendResponse(int fd);
};

#endif
