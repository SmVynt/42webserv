#ifndef SESSION_HPP
# define SESSION_HPP

# include <string>
# include <vector>
# include <fstream>
# include <time.h>
# include "Logger.hpp"

struct Cookie {
	std::string	name;
	std::string	value;
};

class Session {
	public:
		Session();
		Session(const Session &other);
		Session &operator = (const Session &other);
		~Session();

		std::string	getId() const;

		std::string	generateSessionId();
		Cookie		createCookie(const std::string &line);
		std::string	cookieToString(const Cookie &cookie);
		void		addCookie(const Cookie &cookie);
	private:
		std::string			_id;
		time_t				_created_at;
		std::vector<Cookie>	_cookies;
};


#endif

