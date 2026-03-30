#pragma once

#include "hub.hpp"

#include "Response.hpp"
#include "Request.hpp"
#include "CGI.hpp"
#include "utils.hpp"

class CGIexecutor;

/**
 * @brief Handles URI resolution and HTTP method dispatch for incoming requests.
 */
class RequestHandler{
	private:
		/**
		 * @brief Handles `GET`/`HEAD` access to files and directories.
		 * @param req Parsed client request.
		 * @param loc Matched location configuration.
		 * @return Response generated for the request.
		 */
		static Response			handleGet(const Request &req, const Location &loc);

		/**
		 * @brief Handles `POST` requests, including multipart uploads.
		 * @param req Parsed client request.
		 * @param loc Matched location configuration.
		 * @return Response generated for the request.
		 */
		static Response			handlePost(const Request &req, const Location &loc);

		/**
		 * @brief Handles `DELETE` requests for files in allowed locations.
		 * @param req Parsed client request.
		 * @param loc Matched location configuration.
		 * @return Response generated for the request.
		 */
		static Response			handleDelete(const Request &req, const Location &loc);

		/**
		 * @brief Returns MIME type based on file extension.
		 * @param path File path.
		 * @return MIME content type.
		 */
		static std::string		getMimeType(const std::string &path);

		/**
		 * @brief Checks whether a path exists and is a directory.
		 * @param path Filesystem path.
		 * @return true if directory exists, false otherwise.
		 */
		static bool				isDirectory(const std::string &path);

		/**
		 * @brief Checks whether a path exists and is a regular file.
		 * @param path Filesystem path.
		 * @return true if regular file exists, false otherwise.
		 */
		static bool				fileExists(const std::string &path);

		/**
		 * @brief Builds an HTML autoindex page for a directory.
		 * @param path Directory path on disk.
		 * @param uri Request URI used for links.
		 * @return Generated HTML content.
		 */
		static std::string		generateAutoindex(const std::string &path, const std::string &uri);

		/**
		 * @brief Parses and stores multipart/form-data uploads.
		 * @param req Parsed client request.
		 * @param loc Matched location configuration.
		 * @param content_type Request `Content-Type` header value.
		 * @param res Response object updated by this routine.
		 * @return Final upload handling response.
		 */
		static Response			handleMultipartUpload(const Request &req, const Location &loc, const std::string &content_type, Response &res);

		public:
		/**
		 * @brief Finds the best matching location for a URI.
		 * @param uri Request URI.
		 * @param config Active server configuration.
		 * @return Matching location pointer, or `nullptr`.
		 */
		static const Location	*findLocation(const std::string &uri, const ServerConfig &config);

		/**
		 * @brief Resolves location with method-aware fallback behavior.
		 * @param uri Request URI.
		 * @param method HTTP method.
		 * @param config Active server configuration.
		 * @return Resolved location pointer, or `nullptr`.
		 */
		static const Location	*resolveLocation(const std::string &uri, const std::string &method, const ServerConfig &config);

		/**
		 * @brief Dispatches request processing to the corresponding method handler.
		 * @param req Parsed request object.
		 * @param config Active server configuration.
		 * @return Final response for the request.
		 */
		static Response			handleRequest(Request &req, const ServerConfig &config);

		/**
		 * @brief Checks whether the request targets a configured CGI extension.
		 * @param req Parsed request object.
		 * @param loc Matched location configuration.
		 * @return true if CGI should be used, false otherwise.
		 */
		static bool				isCgiRequest(const Request &req, const Location &loc);

		/**
		 * @brief Creates and starts CGI execution for the request.
		 * @param req Parsed request object.
		 * @param loc Matched location configuration.
		 * @param config Active server configuration.
		 * @return Started CGI executor pointer, or `NULL` on failure.
		 */
		static CGIexecutor		*handleCgi(const Request &req, const Location &loc, const ServerConfig &config);

		/**
		 * @brief Parses raw CGI output into an HTTP response.
		 * @param raw_output Raw CGI stdout payload.
		 * @return Parsed response object.
		 */
		static Response			parseCgiOutput(const std::string& raw_output);

};

