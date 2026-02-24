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
				res.setStatusCode(403);
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

Response RequestHandler::handlePost(const Request &req, const Location &loc){
	Response res;

	if (!loc.upload_dir.has_value()){
		res.setStatusCode(403);
		return res;
	}

	if (loc.cgi_path.has_value() && !loc.cgi_ext.value_or("").empty()){
		// return handleCgi(req, loc); or smth like that
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
