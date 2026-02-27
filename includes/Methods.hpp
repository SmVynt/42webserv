#pragma once

#include "Response.hpp"
#include "Request.hpp"
#include "CGI.hpp"
#include "utils.hpp"
#include <filesystem>
#include <unistd.h>

class CGIexecutor;

class RequestHandler{
	private:
		static Response			handleGet(const Request &req, const Location &loc);
		static Response			handlePost(const Request &req, const Location &loc);
		static Response			handleDelete(const Request &req, const Location &loc);

		static CGIexecutor		*handleCgi(const Request &req, const Location &loc, const ServerConfig &config);

		static Response			parseCgiOutput(const std::string& raw_output);

		static std::string		getMimeType(const std::string &path);
		static bool				isCgiRequest(const Request &req, const Location &loc);
		static bool				isDirectory(const std::string &path);
		static bool				fileExists(const std::string &path);

		static std::string		generateAutoindex(const std::string &path, const std::string &uri);

		static Response			handleMultipartUpload(const Request &req, const Location &loc, const std::string &content_type, Response &res);

		public:
		static const Location	*findLocation(const std::string &uri, const ServerConfig &config);
		static Response			handleRequest(Request &req, const ServerConfig &config);

};

