#ifndef SESSION_HPP
# define SESSION_HPP

# include <string>
# include <fstream>
# include <time.h>
# include "Logger.hpp"

class Session {
	public:
		Session();
		Session(const Session &other);
		Session &operator = (const Session &other);
		~Session();

		std::string	getId() const;
		void		touch();
		bool		isExpired(time_t timeout) const;
	private:
		std::string	generateSessionId();

		std::string	_id;
		time_t		_created_at;
		time_t		_last_accessed;
};


#endif

