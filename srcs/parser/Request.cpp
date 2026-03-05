#include "Request.hpp"

Request::Request(): _state(METHOD_LINE), _error_code(0) {}

Request::~Request() {}

bool	Request::isFinished() const { return _state == DONE || _state == ERROR; }

void	Request::setMaxBodySize(const unsigned long &num) { _max_body_size = num; }

std::string	Request::getBody() const {return _body; }
std::string	Request::getMethod() const {return _method; }
std::string	Request::getHttpVersion() const {return _http_version; }
std::string	Request::getPath() const {return _path; }
std::map<std::string, std::string>	Request::getHeaders() const {return _headers; }
std::string	Request::getHeaders(const std::string& key) const
{
	auto it = _headers.find(key);
	if (it == _headers.end())
		return "";
	return it->second;
}
Request::State	Request::getState() const { return _state; }
int		Request::getErrorCode() const { return _error_code; }

void	Request::consume(const std::string &new_chunk){
	// Check if adding this chunk would exceed max body size
	if (_raw_storage.size() + new_chunk.size() > _max_body_size && _max_body_size > 0) {
		_state = ERROR;
		_error_code = 413;
		return;
	}
	_raw_storage += new_chunk;


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
				validate();
				if (_headers.count("transfer-encoding") && _headers["transfer-encoding"] == "chunked")
					_state = CHUNK_SIZE;
				else if (_headers.count("content-length"))
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
		else if (_state == CHUNK_SIZE){
			size_t pos = _raw_storage.find("\r\n");
			if (pos == std::string::npos)
				break;
			std::string hex_str = _raw_storage.substr(0, pos);
			try{
				_current_chunk_size = std::stoul(hex_str, nullptr, 16);
			} catch(...){
				_state = ERROR;
				_error_code = 400;
				break;
			}
			_raw_storage.erase(0, pos + 2);
			_state = (_current_chunk_size == 0) ? DONE : CHUNK_DATA;
		}
		else if (_state == CHUNK_DATA){
			if (_raw_storage.size() < _current_chunk_size + 2)
				break;
			if (_raw_storage.substr(_current_chunk_size, 2) != "\r\n") {
				_state = ERROR;
				_error_code = 400;
				break;
			}
			_body.append(_raw_storage.substr(0, _current_chunk_size));
			_raw_storage.erase(0, _current_chunk_size + 2);
			_state = CHUNK_SIZE;
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

void	Request::validate(){
	if (_headers.find("host") == _headers.end()){
		_state = ERROR;
		_error_code = 400;
		return ;
	}
	if (_headers.count("content-length")){
		try{
			unsigned long cl = std::stoul(_headers["content-length"]);
			if (cl > _max_body_size){
				_state = ERROR;
				_error_code = 413;
				return;
			}
		} catch (...){
			_state = ERROR;
			_error_code = 400;
		}
	}
}

bool Request::shouldKeepAlive() const{
	std::map<std::string, std::string>::const_iterator it = _headers.find("connection");
	if (it != _headers.end()){
		if (it->second == "close")
			return false;
		if (it->second == "keep-alive")
			return true;
	}
	return _http_version == "HTTP/1.1";
}

std::string	Request::getClientIP() const { return _client_ip; }

void		Request::setClientIP(const std::string &ip) { _client_ip = ip; }
