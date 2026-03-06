#include "Response.hpp"

Response::Response(): _version("HTTP/1.1"), _status_code(200) {}

Response::~Response() {}

void	Response::setStatusCode(int code) { _status_code = code; }

void	Response::setBody(const std::string &body) { _body = body; }

void	Response::addHeader(const std::string &key, const std::string &value) { _headers[key] = value; }

void	Response::setConnectionHeader(bool keep_alive){
	if (keep_alive)
		addHeader("Connection", "keep-alive");
	else
		addHeader("Connection", "close");
}

const std::map<int, std::string>	Response::_status_messages = Response::_initStatusMessages();

std::map<int, std::string>	Response::_initStatusMessages(){
	std::map<int, std::string> m;
	m[200] = "OK";
	m[201] = "Created";
	m[204] = "No Content";
	m[301] = "Moved Permanently";
	m[302] = "Found";
	m[307] = "Temporary Redirect";
	m[308] = "Permanent Redirect";
	m[400] = "Bad Request";
	m[403] = "Forbidden";
	m[404] = "Not Found";
	m[405] = "Method Not Allowed";
	m[413] = "Payload Too Large";
	m[500] = "Internal Server Error";
	m[501] = "Not Implemented";
	m[502] = "Bad Gateway";
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

std::string	Response::getStatusMessage(int code){
	if (_status_messages.count(code))
		return _status_messages.at(code);
	return "Unknown Error";
}

void Response::makeDefaultError(int code) {
	_status_code = code;
	std::string msg = getStatusMessage(code);
	_body = "<html><head><title>" + std::to_string(code) + " " + msg + "</title></head>"
			"<body><center><h1>" + std::to_string(code) + " " + msg + "</h1></center>"
			"<hr><center>Webslave/1.0</center></body></html>";
	addHeader("Content-Type", "text/html");
}

void		Response::updateSentBytes(size_t sent) { _bytes_sent += sent; }

int			Response::getStatusCode() const { return _status_code; }

const char	*Response::getUnsentData() const { return _full_response.c_str() + _bytes_sent; }

size_t		Response::getRemainingSize() const { return _full_response.size() - _bytes_sent; }

bool		Response::isFinished() const { return !_full_response.empty() && _bytes_sent >= _full_response.size(); }
