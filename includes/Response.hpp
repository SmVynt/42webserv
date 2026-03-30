#pragma once

#include "hub.hpp"

#include "Request.hpp"
#include "Config.hpp"


class Response {
private:
	std::string	_version;
	int			_status_code;
	std::string	_body;
	std::map<std::string, std::string>	_headers;

	std::string	_full_response;
	size_t		_bytes_sent;

	const static std::map<int, std::string>	_status_messages;

	/**
	 * @brief Builds the default HTTP status-code to reason-phrase map.
	 * @return Map containing status codes and their message strings.
	 */
	static std::map<int, std::string>		_initStatusMessages();

	/**
	 * @brief Builds the complete HTTP response buffer from status, headers, and body.
	 * @return Serialized HTTP response string.
	 */
	std::string build();

public:
	/**
	 * @brief Constructs a response object with default HTTP state.
	 */
	Response();

	/**
	 * @brief Destroys the response object.
	 */
	~Response();

	void				setStatusCode(int code);
	void				setBody(const std::string &body);
	void				setConnectionHeader(bool keep_alive);

	static std::string	getStatusMessage(int code);
	int					getStatusCode() const;
	const char			*getUnsentData() const;
	size_t				getRemainingSize() const;

	/**
	 * @brief Finalizes and caches the full response before transmission.
	 */
	void				prepare();

	/**
	 * @brief Advances the sent-bytes cursor after a socket send operation.
	 * @param sent Number of bytes successfully sent.
	 */
	void				updateSentBytes(size_t sent);

	/**
	 * @brief Indicates whether the full response payload has been transmitted.
	 * @return true when all bytes have been sent, false otherwise.
	 */
	bool				isFinished() const;

	/**
	 * @brief Adds or replaces a response header field.
	 * @param key Header name.
	 * @param value Header value.
	 */
	void				addHeader(const std::string &key, const std::string &value);

	/**
	 * @brief Builds a default error response for a given HTTP status code.
	 * @param code HTTP error status code.
	 */
	void				makeDefaultError(int code);
};
