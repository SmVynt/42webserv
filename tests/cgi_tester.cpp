#include "../includes/CGI.hpp"

int main(int argc, char** argv) {
	if (argc < 2) {
		std::cerr << "Usage: " << argv[0] << " <script_path> [query_string] [post_data] [timeout]" << std::endl;
		std::cerr << "Examples:" << std::endl;
		std::cerr << "  " << argv[0] << " cgi-bin/test.py" << std::endl;
		std::cerr << "  " << argv[0] << " cgi-bin/test.py \"name=John&age=30\"" << std::endl;
		std::cerr << "  " << argv[0] << " cgi-bin/test.py \"\" \"name=John&age=30\"" << std::endl;
		return 1;
	}

	// Check Overloads
	if (argc == 2)
		return runCGI(argv[1], 10);

	if (argc == 3)
		return runCGI(argv[1], (strlen(argv[2]) > 0) ? argv[2] : "", 10);

	return runCGI(argv[1],
		(argc > 2 && strlen(argv[2]) > 0) ? argv[2] : "",
		(argc > 3) ? argv[3] : "",
		(argc > 4) ? atoi(argv[4]) : 10);
}
