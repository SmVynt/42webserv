#pragma once

#include "hub.hpp"

class Request {
public:
	/**
	 * @brief Parsing state machine for an HTTP request.
	 */
	enum State { METHOD_LINE, HEADERS, BODY, CHUNK_SIZE, CHUNK_DATA, DONE, ERROR };

private:
	State		_state;
	std::string	_raw_storage;
	int			_error_code;

	unsigned long	_max_body_size;
	size_t			_content_length;
	size_t			_current_chunk_size;

	std::string	_method;
	std::string	_path;
	std::string	_http_version;
	std::string	_body;
	std::map<std::string, std::string>	_headers;
	std::string	_client_ip;
	bool		_expect_continue;

public:
	/**
	 * @brief Constructs a new empty HTTP request parser.
	 */
	Request();

	/**
	 * @brief Destroys the request instance.
	 */
	~Request();

	/**
	 * @brief Consumes raw bytes and advances the HTTP parsing state machine.
	 * @param new_chunk Pointer to incoming data buffer.
	 * @param chunk_size Number of bytes available in @p new_chunk.
	 */
	void	consume(const char *new_chunk, size_t chunk_size);

	/**
	 * @brief Consumes a string chunk and forwards it to the byte-buffer parser.
	 * @param new_chunk Incoming request data.
	 */
	void	consume(const std::string &new_chunk);

	/**
	 * @brief Parses and validates the HTTP request line.
	 * @param line Raw request line in the form "METHOD path HTTP/version".
	 */
	void	parseRequestLine(const std::string &line);

	/**
	 * @brief Parses a single header line and stores it in lowercase-key form.
	 * @param line Header line in the form "Key: Value".
	 */
	void	parseHeaders(const std::string &line);

	/**
	 * @brief Validates mandatory headers and protocol expectations.
	 */
	void	validate();

	/**
	 * @brief Indicates whether parsing reached a terminal state.
	 * @return true when state is DONE or ERROR, false otherwise.
	 */
	bool	isFinished() const;

	/**
	 * @brief Evaluates if the connection should remain open after this request.
	 * @return true for keep-alive behavior, false to close the connection.
	 */
	bool	shouldKeepAlive() const;

	/**
	 * @brief Reports and clears a pending "100-continue" expectation flag.
	 * @return true if a continue response should be sent, false otherwise.
	 */
	bool	needsContinue();

	void				setMaxBodySize(const unsigned long &num);
	void				setClientIP(const std::string &ip);

	std::string			getMethod() const;
	std::string			getPath() const;
	std::string			getHttpVersion() const;
	const std::string	&getBody() const;
	std::string			getClientIP() const;
	State				getState() const;
	int					getErrorCode() const;

	std::map<std::string, std::string>	getHeaders() const;
	std::string							getHeaders(const std::string& key) const;


};
