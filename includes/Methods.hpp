#pragma once

#include "Response.hpp"
#include "Request.hpp"
#include "utils.hpp"
#include <filesystem>
#include <unistd.h>

class RequestHandler{
	private:
		static Response			handleGet(const Request &req, const Location &loc);
		static Response			handlePost(const Request &req, const Location &loc);
		static Response			handleDelete(const Request &req, const Location &loc);

		static std::string		getMimeType(const std::string &path);
		static bool				isDirectory(const std::string &path);
		static bool				fileExists(const std::string &path);
		static const Location	*findLocation(const std::string &uri, const ServerConfig &config);

		static std::string		generateAutoindex(const std::string &path, const std::string &uri);

	public:
		static Response			handleRequest(Request &req, const ServerConfig &config);

};

