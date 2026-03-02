#ifndef REQUEST_HPP
# define REQUEST_HPP

#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <sstream>

class Request {
public:
	enum State { METHOD_LINE, HEADERS, BODY, CHUNK_SIZE, CHUNK_DATA, DONE, ERROR };

private:
	State		_state;
	std::string	_raw_storage;
	int			_error_code;

	unsigned long	_max_body_size;
	size_t			_current_chunk_size;

	std::string	_method;
	std::string	_path;
	std::string	_http_version;
	std::string	_body;
	std::map<std::string, std::string>	_headers;
	std::string	_client_ip;


public:
	Request();
	~Request();

	void	consume(const std::string &new_chunk);
	void	parseRequestLine(const std::string &line);
	void	parseHeaders(const std::string &line);
	void	validate();

	bool	isFinished() const;
	bool	shouldKeepAlive() const;

	void	setMaxBodySize(const unsigned long &num);

	std::string	getMethod() const;
	std::string	getPath() const;
	std::string	getHttpVersion() const;
	std::string	getBody() const;
	std::string	getClientIP() const;
	void		setClientIP(const std::string &ip);
	std::map<std::string, std::string>	getHeaders() const;
	std::string	getHeaders(const std::string& key) const;
	State	getState() const;
	int		getErrorCode() const;

};

void reqHardcode();
void reqChunkedHardcode();

#endif
