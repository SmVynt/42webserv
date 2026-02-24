#ifndef RESPONSE_HPP
# define RESPONSE_HPP

# include "Request.hpp"
# include "Config.hpp"

#include <sys/types.h>
#include <sys/socket.h>

class Response {
private:
	std::string	_version;
	int			_status_code;
	std::string	_body;
	std::map<std::string, std::string>	_headers;

	std::string	_full_response;
	size_t		_bytes_sent;

	const static std::map<int, std::string>	_status_messages;
	static std::map<int, std::string>		_initStatusMessages();

	std::string build();

public:
	Response();
	~Response();

	void		setStatusCode(int code);
	void		setBody(const std::string &body);
	void		addHeader(const std::string &key, const std::string &value);
	void		setConnectionHeader(bool keep_alive);

	void		prepare();
	void		updateSentBytes(size_t sent);

	int			getStatusCode() const;
	const char	*getUnsentData() const;
	size_t		getRemainingSize() const;
	bool		isFinished() const;

	static std::string	getStatusMessage(int code);
	void				makeDefaultError(int code);
};

#endif
