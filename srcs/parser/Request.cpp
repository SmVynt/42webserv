#include "Request.hpp"

Request::Request(): _state(METHOD_LINE) {}

Request::~Request() {}

bool	Request::isFinished() const { return _state == DONE || _state == ERROR; }

std::string	Request::getBody() const {return _body; }
std::string	Request::getMethod() const {return _method; }
std::string	Request::getHttpVersion() const {return _http_version; }
std::string	Request::getPath() const {return _path; }
std::map<std::string, std::string>	Request::getHeaders() const {return _headers; }

void	Request::consume(const std::string &new_chunk){
	_raw_storage += new_chunk;

	while (true){
		if (_state == METHOD_LINE){
			size_t pos = _raw_storage.find("\r\n");
			if (pos == std::string::npos)
				break;

			std::string line = _raw_storage.substr(0, pos);
			_raw_storage.erase(0, pos + 2);
			//todo parse_line
			_state = HEADERS;
		}
		else if (_state = HEADERS){
			//todo parse_headers
		}
	}
}