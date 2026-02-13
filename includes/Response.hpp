#ifndef RESPONSE_HPP
# define RESPONSE_HPP

# include "Request.hpp"
# include "Config.hpp"

class Response {
private:
	std::string	_version;
	int			_status_code;
	std::string	_body;
	std::map<std::string, std::string>	_headers;

	const static std::map<int, std::string>	_status_messages;
	static std::map<int, std::string>	_initStatusMessages();

public:
	Response();
	void setStatusCode(int code);
	void setBody(const std::string &body);
	void addHeader(const std::string &key, const std::string &value);
	std::string build();
};

#endif
