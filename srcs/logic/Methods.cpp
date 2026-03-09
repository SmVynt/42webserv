#include "Methods.hpp"

bool	RequestHandler::isDirectory(const std::string &path){
	return std::filesystem::exists(path) && std::filesystem::is_directory(path);
}

bool	RequestHandler::fileExists(const std::string &path){
	return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
}

std::string RequestHandler::getMimeType(const std::string &path){
	size_t pos = path.find_last_of(".");
	if (pos == std::string::npos)
		return "application/octet-stream";

	std::string ext = path.substr(pos);
	if (ext == ".html" || ext == ".htm")
		return "text/html";
	else if (ext == ".css")
		return "text/css";
	else if (ext == ".js")
		return "application/javascript";
	else if (ext == ".jpg" || ext == ".jpeg")
		return "image/jpeg";
	else if (ext == ".png")
		return "image/png";
	else if (ext == ".gif")
		return "image/gif";
	else if (ext == ".json")
		return "application/json";
	else if (ext == ".txt")
		return "text/plain";
	return "application/octet-stream";
}

bool RequestHandler::isCgiRequest(const Request &req, const Location &loc){
	if (!loc.cgi_ext.has_value())
		return false;
	std::string path = req.getPath();
	size_t query_pos = path.find('?');
	std::string clean_path = (query_pos == std::string::npos) ? path : path.substr(0, query_pos);

	size_t dot_pos = clean_path.find_last_of(".");
	if (dot_pos == std::string::npos)
		return false;
	return clean_path.substr(dot_pos) == loc.cgi_ext.value();
}

const Location *RequestHandler::findLocation(const std::string &uri, const ServerConfig &config){
	const Location* best_match = nullptr;
	const Location* extension_match = nullptr;
	size_t max_len = 0;

	for (size_t i = 0; i < config.locations.size(); i++) {
		const Location &loc = config.locations[i];

		// Check if path starts with .dot (extension-based location)
		if (!loc.path.empty() && loc.path[0] == '.') {
			// Extension-based location matching
			size_t dot_pos = uri.find_last_of(".");
			if (dot_pos != std::string::npos) {
				std::string file_ext = uri.substr(dot_pos);
				if (file_ext == loc.path) {
					// Extension match found - store but don't return yet
					// We'll return it only if method is allowed
					extension_match = &loc;
				}
			}
		}
		else if (uri.compare(0, loc.path.length(), loc.path) == 0) {
			// Path-based location matching
			if (loc.path.length() > max_len) {
				max_len = loc.path.length();
				best_match = &loc;
			}
		}
	}

	// Return extension match if found, otherwise path match
	return extension_match ? extension_match : best_match;
}

const Location *RequestHandler::resolveLocation(const std::string &uri, const std::string &method, const ServerConfig &config){
	const Location *loc = findLocation(uri, config);

	if (!loc)
		return nullptr;

	// Check if method is allowed for this location
	bool method_allowed = false;
	for (const auto& m : loc->methods) {
		if (m == method) {
			method_allowed = true;
			break;
		}
	}

	// If extension-based location doesn't allow this method, find path-based location
	if (!method_allowed && !loc->path.empty() && loc->path[0] == '.') {
		const Location* fallback = nullptr;
		size_t max_len = 0;

		for (size_t i = 0; i < config.locations.size(); i++) {
			const Location &candidate = config.locations[i];
			// Only consider path-based locations (not extension-based)
			if (candidate.path.empty() || candidate.path[0] == '.')
				continue;

			if (uri.compare(0, candidate.path.length(), candidate.path) == 0) {
				if (candidate.path.length() > max_len) {
					max_len = candidate.path.length();
					fallback = &candidate;
				}
			}
		}

		if (fallback)
			loc = fallback;
	}

	return loc;
}

Response RequestHandler::handleGet(const Request &req, const Location &loc) {
	Response res;

	std::filesystem::path root_path(loc.root);
	std::string request_path = req.getPath();

	std::string rel_uri = request_path.substr(loc.path.length());
	if (!rel_uri.empty() && rel_uri[0] == '/')
		rel_uri.erase(0, 1);

	std::filesystem::path full_path = root_path / rel_uri;

	if (!std::filesystem::exists(full_path)) {
		res.setStatusCode(404);
		return res;
	}

	if (isDirectory(full_path)) {
		if (!loc.index.empty()) {
			std::filesystem::path index_path = full_path / loc.index;
			if (fileExists(index_path))
				full_path = index_path;
			else if (loc.autoindex) {
				res.setStatusCode(200);
				res.setBody(generateAutoindex(full_path.string(), request_path));
				res.addHeader("Content-Type", "text/html");
				return res;
			}
			else {
				res.setStatusCode(404);
				return res;
			}
		}
		else if (loc.autoindex) {
			res.setStatusCode(200);
			res.setBody(generateAutoindex(full_path.string(), request_path));
			res.addHeader("Content-Type", "text/html");
			return res;
		} else {
			res.setStatusCode(403);
			return res;
		}
	}

	if (access(full_path.c_str(), R_OK) != 0) {
		res.setStatusCode(403);
		return res;
	}

	std::string content = loadFile(full_path.string());
	res.setStatusCode(200);
	res.setBody(content);
	res.addHeader("Content-Type", getMimeType(full_path.string()));

	return res;
}

Response RequestHandler::handleRequest(Request &req, const ServerConfig &config){
	Response res;
	const Location *loc = resolveLocation(req.getPath(), req.getMethod(), config);

	if (!loc){
		res.setStatusCode(404);
		return res;
	}

	if (!loc->redirection.second.empty()){
		int code = loc->redirection.first;
		res.setStatusCode(code ? code : 302);
		res.addHeader("Location", loc->redirection.second);
		res.setBody("Redirecting to " + loc->redirection.second);
		return res;
	}

	bool allowed_method = false;
	std::string allowed_list = "";
	for (size_t i = 0; i < loc->methods.size(); i++){
		if (i > 0)
			allowed_list += ", ";

		allowed_list += loc->methods[i];
		if (loc->methods[i] == req.getMethod()){
			allowed_method = true;
		}
	}

	if (!allowed_method){
		res.setStatusCode(405);
		res.addHeader("Allow", allowed_list);
		res.setBody("405 Method Not Allowed: Use " + allowed_list);
		return res;
	}

	if (req.getMethod() == "GET" || req.getMethod() == "HEAD")
		res = handleGet(req, *loc);
	else if (req.getMethod() == "POST")
		res = handlePost(req, *loc);
	else if (req.getMethod() == "DELETE")
		res = handleDelete(req, *loc);
	else
		res.setStatusCode(501);

	bool keep_alive = req.shouldKeepAlive();
	res.setConnectionHeader(keep_alive);

	return res;
}

Response RequestHandler::handleMultipartUpload(const Request &req, const Location &loc, const std::string &content_type, Response &res){
	size_t boundary_pos = content_type.find("boundary=");
	if (boundary_pos == std::string::npos){
		res.setStatusCode(400);
		return res;
	}

	std::string boundary = "--" + content_type.substr(boundary_pos + 9);
	std::string body = req.getBody();

	size_t start = body.find(boundary);
	if (start == std::string::npos){
		res.setStatusCode(400);
		return res;
	}

	size_t header_end = body.find("\r\n\r\n", start);
	if (header_end == std::string::npos){
		res.setStatusCode(400);
		return res;
	}

	std::string part_header = body.substr(start, header_end - start);
	size_t filename_pos = part_header.find("filename=\"");
	if (filename_pos == std::string::npos){
		res.setStatusCode(400);
		return res;
	}
	filename_pos += 10;
	size_t filename_end = part_header.find("\"", filename_pos);
	std::string filename = part_header.substr(filename_pos, filename_end - filename_pos);
	std::string safe_filename = std::filesystem::path(filename).filename().string();

	size_t data_start = header_end + 4;
	size_t data_end = body.find(boundary, data_start);
	if (data_end == std::string::npos){
		res.setStatusCode(400);
		return res;
	}

	std::string file_data = body.substr(data_start, data_end - data_start - 2);

	std::filesystem::path upload_path = loc.upload_dir.value();
	if (!std::filesystem::exists(upload_path))
		std::filesystem::create_directories(upload_path);

	upload_path /= safe_filename;

	std::ofstream out_file(upload_path, std::ios::binary);
	if (!out_file){
		res.setStatusCode(500);
		return res;
	}

	out_file << file_data;
	out_file.close();

	res.setStatusCode(201);
	res.setBody("File upload succesfully: " + safe_filename);
	return res;
}

CGIexecutor *RequestHandler::handleCgi(const Request &req, const Location &loc, const ServerConfig &config) {
	std::string full_path = req.getPath();
	size_t query_pos = full_path.find('?');
	std::string script_uri = (query_pos == std::string::npos) ? full_path : full_path.substr(0, query_pos);
	std::string query_string = (query_pos == std::string::npos) ? "" : full_path.substr(query_pos + 1);

	// Handle extension-based locations differently
	std::string script_path;
	if (!loc.path.empty() && loc.path[0] == '.') {
		// Extension-based location
		// Check if there's a matching path-based location to use for path resolution
		const Location* path_loc = nullptr;
		size_t max_len = 0;

		for (size_t i = 0; i < config.locations.size(); i++) {
			const Location &candidate = config.locations[i];
			// Only consider path-based locations
			if (candidate.path.empty() || candidate.path[0] == '.')
				continue;

			if (script_uri.compare(0, candidate.path.length(), candidate.path) == 0) {
				if (candidate.path.length() > max_len) {
					max_len = candidate.path.length();
					path_loc = &candidate;
				}
			}
		}

		// If we found a matching path-based location, use its root and path logic
		if (path_loc) {
			script_path = path_loc->root + script_uri.substr(path_loc->path.length());
		} else {
			// No matching path-based location, use extension location root with full path
			script_path = loc.root + script_uri;
		}
	} else {
		// Path-based location: strip the location path prefix
		script_path = loc.root + script_uri.substr(loc.path.length());
	}

	CGIconfig cgi_config(script_path, script_uri, query_string, req.getBody(), config);
	CGIexecutor *executor = new CGIexecutor(cgi_config);

	std::map<std::string, std::string> req_headers = req.getHeaders();
	for (std::map<std::string, std::string>::const_iterator it = req_headers.begin(); it != req_headers.end(); ++it){
		executor->setHttpHeader(it->first, it->second);
	}
	executor->setEnvKey("REQUEST_METHOD", req.getMethod());
	executor->setEnvKey("REMOTE_ADDR", req.getClientIP());
	executor->setEnvKeySafe("REMOTE_HOST", req.getClientIP());

	if (executor->start() != 0) {
		delete executor;
		return NULL;
	}

	return executor;
}

Response RequestHandler::parseCgiOutput(const std::string& raw_output){
	Response res;
	size_t sep = raw_output.find("\r\n\r\n");
	size_t sep_len = 4;

	if (sep == std::string::npos){
		sep = raw_output.find("\n\n");
		sep_len = 2;
	}

	if (sep != std::string::npos){
		std::string headers_part = raw_output.substr(0, sep);
		std::string body_part = raw_output.substr(sep + sep_len);

		std::istringstream stream(headers_part);
		std::string header_line;
		while (std::getline(stream, header_line)){
			if (!header_line.empty() && header_line.back() == '\r')
				header_line.pop_back();

			size_t colon = header_line.find(':');
			if (colon != std::string::npos){
				std::string key = header_line.substr(0, colon);
				std::string val = header_line.substr(colon + 1);
				val.erase(0, val.find_first_not_of(" "));

				if (key == "Status")
					res.setStatusCode(std::atoi(val.c_str()));
				else
					res.addHeader(key, val);
			}
		}
		res.setStatusCode(res.getStatusCode() ? res.getStatusCode() : 200);
		res.setBody(body_part);
	}
	else{
		res.setStatusCode(502);
		res.setBody("CGI Error: Missing header separator");
	}
	return res;
}

Response RequestHandler::handlePost(const Request &req, const Location &loc){
	Response res;

	if (!loc.upload_dir.has_value()){
		res.setStatusCode(403);
		return res;
	}

	std::string content_type = "";
	std::map<std::string, std::string> headers = req.getHeaders();
	if (headers.count("content-type"))
		content_type = headers["content-type"];

	if (content_type.find("multipart/form-data") != std::string::npos)
		return handleMultipartUpload(req, loc, content_type, res);


	if (!req.getBody().empty()){
		res.setStatusCode(200);
		res.setBody("Post data received. Size: " + std::to_string(req.getBody().size()));
	}
	else
		res.setStatusCode(204);

	return res;
}

Response RequestHandler::handleDelete(const Request &req, const Location &loc){
	Response res;

	std::filesystem::path root_path(loc.root);
	std::string request_path = req.getPath();

	std::string rel_uri = request_path.substr(loc.path.length());
	if (!rel_uri.empty() && rel_uri[0] == '/')
		rel_uri.erase(0, 1);

	std::filesystem::path full_path = (root_path / rel_uri).lexically_normal();

	if (!std::filesystem::exists(full_path)){
		res.setStatusCode(404);
		return res;
	}

	if (isDirectory(full_path)){
		res.setStatusCode(403);
		return res;
	}

	if (access(full_path.c_str(), W_OK) != 0){
		res.setStatusCode(403);
		return res;
	}

	try{
		if (std::filesystem::remove(full_path))
			res.setStatusCode(204);
		else
			res.setStatusCode(500);
	} catch (const std::filesystem::filesystem_error& e){
			res.setStatusCode(500);
	}

	return res;
}

std::string	RequestHandler::generateAutoindex(const std::string &path, const std::string &uri){
	std::string html = "<html><head><title>Index of " + uri + "</title></head><body>";
	html += "<h1>Index of " + uri + "</h1><hr><pre>";

	try{
		for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(path)){
			std::string name = entry.path().filename().string();
			if (entry.is_directory())
				name += "/";

			std::string link = uri;
			if (link.back() != '/')
				link += "/";
			link += name;

			html += "<a href=\"" + link + "\">" + name + "</a>\n";

		}
	} catch (const std::exception &e){
		return "<html><body><h1>Error generating autoindex</h1></body></html>";
	}
	html += "</pre><hr></body></html>";
	return html;
}
