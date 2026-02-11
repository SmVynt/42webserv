#ifndef REQUEST_HPP
# define REQUEST_HPP

#include <string>
#include <map>
#include <vector>

class Request {
public:
	enum State { METHOD_LINE, HEADERS, BODY, DONE, ERROR };

private:
	State		_state;
	std::string	_raw_storage;

	std::string	_method;
	std::string	_path;
	std::string	_http_version;
	std::string	_body;
	std::map<std::string, std::string>	_headers;

public:
	Request();

	void consume(const std::string &new_chunk);

	bool isFinished() const;
};

#endif
