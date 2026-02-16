#include "Response.hpp"

Response::Response(): _version("HTTP/1.1"), _status_code(200) {}

Response::~Response() {}

void	Response::setStatusCode(int code) { _status_code = code; }

void	Response::setBody(const std::string &body) { _body = body; }

void	Response::addHeader(const std::string &key, const std::string &value) { _headers[key] = value; }

const std::map<int, std::string>	Response::_status_messages = Response::_initStatusMessages();

std::map<int, std::string>	Response::_initStatusMessages(){
	std::map<int, std::string> m;
	m[200] = "OK";
	m[201] = "Created";
	m[301] = "Moved Permanently";
	m[400] = "Bad Request";
	m[403] = "Forbidden";
	m[404] = "Not Found";
	m[405] = "Method Not Allowed";
	m[413] = "Payload Too Large";
	m[500] = "Internal Server Error";
	m[501] = "Not Implemented";
	return m;
}

std::string	Response::build(){
	std::stringstream res;

	res << _version << " " << _status_code << " " << _status_messages.at(_status_code) << "\r\n";
	if (!_body.empty() && _headers.find("Content-Length") == _headers.end()){
		addHeader("Content-Length", std::to_string(_body.size()));
	}

	addHeader("Server", "Webslave");

	for (std::map<std::string, std::string>::iterator it = _headers.begin(); it != _headers.end(); it++){
		res << it->first << ": " << it->second << "\r\n";
	}

	res << "\r\n";

	res << _body;

	return res.str();
}

void	Response::prepare(){
	_full_response = build();
	_bytes_sent = 0;
}

int		Response::sendResponse(int fd){
	if (_full_response.empty())
		prepare();
	const char	*remaining_data = _full_response.c_str() + _bytes_sent;
	size_t		remaining_size = _full_response.size() - _bytes_sent;
	ssize_t		sent = send(fd, remaining_data, remaining_size, 0);

	if (sent <= 0)
		return sent;

	_bytes_sent += sent;

	if (_bytes_sent == _full_response.size())
		return 1;
	return 0;
}