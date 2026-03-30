#include "Session.hpp"

Session::Session() : _id(generateSessionId()), _created_at(time(NULL)), _last_accessed(time(NULL)) {
	Logger::info("New session created with ID: " + _id);
}
Session::Session(const Session &other) : _id(other._id), _created_at(other._created_at), _last_accessed(other._last_accessed) {
}
Session &Session::operator = (const Session &other) {
	if (this != &other) {
		_id = other._id;
		_created_at = other._created_at;
		_last_accessed = other._last_accessed;
	}
	return *this;
}
Session::~Session() {
}

std::string	Session::getId() const {
	return _id;
}

std::string	Session::generateSessionId() {
	const char		charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	const size_t	max_index = sizeof(charset) - 1;
	std::string		sessionId;

	// unsigned char buf[32];
	// std::ifstream urandom("/dev/urandom", std::ios::binary);
	// urandom.read(reinterpret_cast<char*>(buf), sizeof(buf));
	for (size_t i = 0; i < 32; i++){
		sessionId += charset[rand() % max_index];
	}
	return sessionId;
}

void	Session::touch() {
	_last_accessed = time(NULL);
}

bool	Session::isExpired(time_t timeout) const {
	return (time(NULL) - _last_accessed) > timeout;
}
