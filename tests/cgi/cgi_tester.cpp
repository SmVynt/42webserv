#include "../../includes/CGI.hpp"
#include "../../includes/Config.hpp"
#include "../../includes/Logger.hpp"
#include <vector>
#include <map>

#define COL_Y "\033[33m"
#define COL_G "\033[32m"
#define COL_R "\033[31m"
#define COL_RESET "\033[0m"

int main() {

	std::vector<CGIexecutor*>	cgi_processes;

	// ServerConfig default_config;
	// default_config.client_timeout = 10;
	// default_config.client_max_body_size = 1024 * 1024;
	std::vector<std::string> tokens = tokenize("config/default.conf");
	Config parser(tokens);
	std::vector<ServerConfig> servers = parser.parse();
	ServerConfig default_config = servers[0]; // Use the first server config for testing

	//python
	cgi_processes.push_back(runCGI("cgi-bin/python/test.py", default_config));
	cgi_processes.push_back(runCGI("cgi-bin/python/test.py", "name=Alice&age=25", default_config));
	cgi_processes.push_back(runCGI("cgi-bin/python/test.py", "", "username=testuser&password=secret123", default_config));
	//php
	cgi_processes.push_back(runCGI("cgi-bin/php/test.php", default_config));
	cgi_processes.push_back(runCGI("cgi-bin/php/test.php", "name=Bob&city=Paris", default_config));
	cgi_processes.push_back(runCGI("cgi-bin/php/test.php", "", "email=test@example.com&message=Hello", default_config));
	//perl
	cgi_processes.push_back(runCGI("cgi-bin/perl/test.pl", default_config));
	//lua
	cgi_processes.push_back(runCGI("cgi-bin/lua/test.lua", default_config));
	// node
	cgi_processes.push_back(runCGI("cgi-bin/node/test.js", default_config));
	// ruby
	cgi_processes.push_back(runCGI("cgi-bin/ruby/test.rb", default_config));
	// shell
	cgi_processes.push_back(runCGI("cgi-bin/shell/test.sh", default_config));
	// timeout tests
	default_config.client_timeout = 2;
	cgi_processes.push_back(runCGI("cgi-bin/python/slow.py", "5", "", default_config)); // should timeout
	default_config.client_timeout = 5;
	cgi_processes.push_back(runCGI("cgi-bin/python/slow.py", "2", "", default_config)); // should complete
	default_config.client_timeout = 3;
	cgi_processes.push_back(runCGI("cgi-bin/python/infinite.py", "", "", default_config)); // should timeout

	// Let's generate the pollfd list and map for quick lookup
	std::vector<struct pollfd> pollfds;
	std::map<int, CGIexecutor*> fd_to_cgi;
	for (auto* cgi : cgi_processes) {
		if (cgi != nullptr) {
			int fd = cgi->getOutputFd();
			if (fd >= 0) {
				pollfds.push_back({fd, POLLIN, 0});
				fd_to_cgi[fd] = cgi;
			}
		} else {
			std::cerr << "Error: Failed to start CGI process" << std::endl;
		}
	}

	// MAIN LOOP: Poll for output and check for completion/timeouts
	while (!pollfds.empty()) {
		int ret = poll(pollfds.data(), pollfds.size(), 100);

		if (ret < 0) {
			std::cerr << "poll() error: " << strerror(errno) << std::endl;
			break;
		}

		// Check for ready file descriptors
		for (size_t i = 0; i < pollfds.size(); ++i) {
			if (pollfds[i].revents & POLLIN) {
				CGIexecutor* cgi = fd_to_cgi[pollfds[i].fd];
				if (cgi) {
					cgi->readOutput(); // Read available output and place it in the buffer.
				}
			}
		}

		// Check for completion and timeouts, remove completed CGIs
		size_t	i;
		for (i=0; i < pollfds.size(); ++i) {
			int fd = pollfds[i].fd;
			CGIexecutor* cgi = fd_to_cgi[fd];

			bool should_remove = false;

			// Check timeout
			if (cgi->checkTimeout()) {
				std::cout << COL_R << "\n=== CGI TIMEOUT ===\n" << COL_RESET;
				std::cout << cgi->getOutput();
				std::cout << COL_Y << "=== CGI Exit Status: 504 (Timeout) ===\n" << COL_RESET;
				std::cout << std::endl;
				cgi->killChildProcess();
				should_remove = true;
			}
			// Check if process completed
			else if (cgi->isComplete()) {
				cgi->readOutput(); // Read any remaining output
				std::cout << COL_G << "\n=== CGI Output ===\n" << COL_RESET;
				std::cout << cgi->getOutput();
				std::cout << COL_Y << "=== CGI Exit Status: " << cgi->getExitStatus() << " ===\n" << COL_RESET;
				std::cout << std::endl;
				should_remove = true;
			}

			if (should_remove) {
				delete cgi;
				fd_to_cgi.erase(fd);
				pollfds.erase(pollfds.begin() + i);
			} else {
				++i;
			}
		}
	}

	return 0;
}
