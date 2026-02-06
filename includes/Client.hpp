#ifndef CLIENT_HPP
# define CLIENT_HPP

#include "Request.hpp"
#include "Response.hpp"

class Client {

	public:
		Client();
		Client(int socket);
		~Client();
};

#endif
