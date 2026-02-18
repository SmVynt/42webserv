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
	if (ext == ".html" || ext == ".hmt")
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
		return "test/plain";
	return "application/octet-stream";
}

const Location *RequestHandler::findLocation(const std::string &uri, const ServerConfig &config){
	const Location* best_match = nullptr;
	size_t max_len = 0;

	for (size_t i = 0; i < config.locations.size(); i++) {
		const Location &loc = config.locations[i];
		if (uri.compare(0, loc.path.length(), loc.path) == 0) {
			if (loc.path.length() > max_len) {
				max_len = loc.path.length();
				best_match = &loc;
			}
		}
	}
	return best_match;
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

	if (std::filesystem::is_directory(full_path)) {
		if (!loc.index.empty()) {
			std::filesystem::path index_path = full_path / loc.index;
			if (std::filesystem::exists(index_path) && std::filesystem::is_regular_file(index_path))
				full_path = index_path;
			else if (loc.autoindex) {
				res.setStatusCode(200);
				//res.setBody(generateAutoindex(full_path.string(), request_path));
				res.addHeader("Content-Type", "text/html");
				return res;
			}
			else {
				res.setStatusCode(403);
				return res;
			}
		}
		else if (loc.autoindex) {
			res.setStatusCode(200);
			//res.setBody(generateAutoindex(full_path.string(), request_path));
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
	const Location *loc = findLocation(req.getPath(), config);

	if (!loc){
		res.setStatusCode(404);
		return res;
	}
	bool allowed_method = false;
	for (size_t i = 0; i < loc->methods.size(); i++) {
		if (loc->methods[i] == req.getMethod()) {
			allowed_method = true;
			break;
		}
	}

	if (!allowed_method){
		res.setStatusCode(405);
		return res;
	}

	if (req.getMethod() == "GET")
		return handleGet(req, *loc);
	else if (req.getMethod() == "POST")
		return handlePost(req, *loc);
	else if (req.getMethod() == "DELETE")
		return handleDelete(req, *loc);

	res.setStatusCode(501);
	return res;
}

Response RequestHandler::handlePost(const Request &req, const Location &loc){
	Response res;
	(void)req;
	(void)loc;
	return res;
}

Response RequestHandler::handleDelete(const Request &req, const Location &loc){
	Response res;
	(void)req;
	(void)loc;
	return res;
}