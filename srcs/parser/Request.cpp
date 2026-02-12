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
	// std::cout << _raw_storage << std::endl;

	while (true){
		// std::cout << _raw_storage << std::endl;
		if (_state == METHOD_LINE){
			size_t pos = _raw_storage.find("\r\n");
			if (pos == std::string::npos)
				break;

			std::string line = _raw_storage.substr(0, pos);
			_raw_storage.erase(0, pos + 2);
			parseRequestLine(line);
			_state = HEADERS;
		}
		else if (_state == HEADERS){

			size_t pos = _raw_storage.find("\r\n");
			if (pos == std::string::npos)
				break;
			std::string line = _raw_storage.substr(0, pos);
			_raw_storage.erase(0, pos + 2);
			if (line.empty()) {
				if (_headers.count("content-length"))
					_state = BODY;
				else if (_headers.count("transfer-encoding") && _headers["transfer-encoding"] == "chunked")
					_state = BODY;
				else
					_state = DONE;

			}
			else
				parseHeaders(line);
		}
		else if (_state == BODY){
			size_t contentLenght = 0;
			try{
				contentLenght = std::stoul(_headers["content-length"]);
			} catch (...){
				_state = ERROR;
				break;
			}
			if (_raw_storage.size() >= contentLenght){
				_body = _raw_storage.substr(0, contentLenght);
				_raw_storage.erase(0, contentLenght);
				_state = DONE;
			}
			break;
		}
		else
			break;
	}
}

void Request::parseHeaders(const std::string &line) {
	size_t pos = line.find(":");
	if (pos == std::string::npos) return;

	std::string key = line.substr(0, pos);
	std::string value = line.substr(pos + 1);

	for (size_t i = 0; i < key.length(); i++)
		key[i] = std::tolower(key[i]);

	size_t first = value.find_first_not_of(" \t");
	if (std::string::npos != first) {
		size_t last = value.find_last_not_of(" \t");
		value = value.substr(first, (last - first + 1));
	}
	else
		value = "";

	_headers[key] = value;
}

void	Request::parseRequestLine(const std::string &line){
	std::istringstream iss(line);
	std::string	method;
	std::string	path;
	std::string	version;

	if (!(iss >> method >> path >> version)){
		_state = ERROR;
		return;
	}
	if (method != "GET" && method != "POST" && method != "DELETE"){
		_state = ERROR;
		return;
	}
	if (version != "HTTP/1.1"){
		_state = ERROR;
		return;
	}

	_method = method;
	_path = path;
	_http_version = version;

}
