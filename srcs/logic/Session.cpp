#include "Session.hpp"

Session::Session() : _id(generateSessionId()), _created_at(time(NULL)) {
	Logger::info("New session created with ID: " + _id);
}
Session::Session(const Session &other) : _id(other._id), _created_at(other._created_at) {
	_cookies = other._cookies;
}
Session &Session::operator = (const Session &other) {
	if (this != &other) {
		_id = other._id;
		_created_at = other._created_at;
		_cookies = other._cookies;
	}
	return *this;
}
Session::~Session() {
	Logger::info("Session with ID: " + _id + " destroyed.");
}

std::string	Session::getId() const {
	return _id;
}

std::string	Session::generateSessionId() {
	const char		charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	const size_t	max_index = sizeof(charset) - 1;
	std::string		sessionId;

	unsigned char buf[32];
	std::ifstream urandom("/dev/urandom", std::ios::binary);
	urandom.read(reinterpret_cast<char*>(buf), sizeof(buf));

	for (size_t i = 0; i < 32; i++){
		sessionId += charset[buf[i] % max_index];
	}
	return sessionId;
}

Cookie	Session::createCookie(const std::string &line) {
	Cookie temp;
	size_t eq_pos = line.find('=');
	if (eq_pos == std::string::npos){
		temp.name = "";
		temp.value = "";
		return temp;
	}

	temp.name = line.substr(0, eq_pos);
	temp.value = line.substr(eq_pos + 1);

	return temp;
}

std::string	Session::cookieToString(const Cookie &cookie) {
	return cookie.name + "=" + cookie.value;
}

void	Session::addCookie(const Cookie &cookie) {
	_cookies.push_back(cookie);
	Logger::info("Added cookie: " + cookie.name + "=" + cookie.value + " to session ID: " + _id);
}
