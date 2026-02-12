#include "../includes/CGI.hpp"
#include <vector>
#include <map>

#define COL_Y "\033[33m"
#define COL_G "\033[32m"
#define COL_R "\033[31m"
#define COL_RESET "\033[0m"

int main() {

	std::vector<CGIexecutor*>	cgi_processes;

	//python
	cgi_processes.push_back(runCGI("cgi-bin/test.py"));
	cgi_processes.push_back(runCGI("cgi-bin/test.py", "name=Alice&age=25"));
	cgi_processes.push_back(runCGI("cgi-bin/test.py", "", "username=testuser&password=secret123"));
	//php
	cgi_processes.push_back(runCGI("cgi-bin/test.php"));
	cgi_processes.push_back(runCGI("cgi-bin/test.php", "name=Bob&city=Paris"));
	cgi_processes.push_back(runCGI("cgi-bin/test.php", "", "email=test@example.com&message=Hello"));
	// shell
	cgi_processes.push_back(runCGI("cgi-bin/test.sh"));
	// timeout tests
	cgi_processes.push_back(runCGI("cgi-bin/slow.py", "5", "", 2)); // should timeout
	cgi_processes.push_back(runCGI("cgi-bin/slow.py", "2", "", 5)); // should complete
	cgi_processes.push_back(runCGI("cgi-bin/infinite.py", "", "", 3)); // should timeout

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
