GREEN			= \033[0;32m
YELLOW			= \033[0;33m
RESET			= \033[0m

NAME			= webserv
CXX				= c++
CXXFLAGS		= -Wall -Wextra -Werror -std=c++17 -Wpedantic
OBJ_DIR			= obj/
INC_DIR			= includes/
SRC_DIR			= srcs/

CORE_SRCS	= main.cpp \
				core/VirtualServer.cpp \
				core/Client.cpp \
				core/Cluster.cpp
PARSER_SRCS	= parser/Config.cpp \
				parser/Request.cpp \
				parser/parserUtils.cpp

LOGIC_SRCS	= logic/Response.cpp \
				logic/Methods.cpp

CGI_SRCS	= cgi/CGI.cpp \
				cgi/cgiUtils.cpp \
				cgi/cgiError.cpp

UTILS_SRCS	= utilities/utils.cpp \
				utilities/Logger.cpp \
				utilities/ResMonitor.cpp

SRCS		= $(CORE_SRCS) $(PARSER_SRCS) $(LOGIC_SRCS) $(CGI_SRCS) $(UTILS_SRCS)

OBJS		= $(addprefix $(OBJ_DIR), $(SRCS:.cpp=.o))

all: $(NAME)

$(NAME): $(OBJS)
	@echo "$(YELLOW)Linking $(NAME)...$(RESET)"
	@$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)
	@echo "$(GREEN)Done! $(NAME) created.$(RESET)"

$(OBJ_DIR)%.o: $(SRC_DIR)%.cpp
	@mkdir -p $(dir $@)
	@echo "Compiling $<..."
	@$(CXX) $(CXXFLAGS) -I$(INC_DIR) -c $< -o $@

clean:
	@echo "$(YELLOW)Removing object files...$(RESET)"
	@rm -rf $(OBJ_DIR)

fclean: clean
	@echo "$(YELLOW)Removing binary...$(RESET)"
	@rm -f $(NAME)

re: fclean all

# Testing
test: all
	@echo "$(YELLOW)Running complete test suite...$(RESET)"
	@cd tests && bash test_suite.sh
	@echo "$(GREEN)Tests completed!$(RESET)"

test-load: all
	@echo "$(YELLOW)Running load tests...$(RESET)"
	@cd tests/load && bash siege_test.sh && bash ab_test.sh && bash concurrent_test.sh
	@echo "$(GREEN)Load tests completed!$(RESET)"

test-memory: all
	@echo "$(YELLOW)Running memory leak tests...$(RESET)"
	@cd tests/memory && bash valgrind_test.sh
	@echo "$(GREEN)Memory tests completed!$(RESET)"

test-edge: all
	@echo "$(YELLOW)Running edge case tests...$(RESET)"
	@cd tests/edge_cases && bash malformed_requests.sh && bash boundary_tests.sh && bash protocol_edge_cases.sh
	@echo "$(GREEN)Edge case tests completed!$(RESET)"

test-static: all
	@echo "$(YELLOW)Running static file serving tests...$(RESET)"
	@cd tests/static && bash serve_test.sh
	@echo "$(GREEN)Static file tests completed!$(RESET)"

test-nginx: all
	@echo "$(YELLOW)Running NGINX comparison...$(RESET)"
	@cd tests/comparison && bash nginx_setup.sh && bash compare_results.sh
	@echo "$(GREEN)Comparison completed!$(RESET)"

test-quick: all
	@echo "$(YELLOW)Running quick validation tests...$(RESET)"
	@cd tests/static && bash serve_test.sh
	@cd tests/edge_cases && bash malformed_requests.sh
	@echo "$(GREEN)Quick tests completed!$(RESET)"

test-cgi:
	@echo "$(YELLOW)Running CGI tests...$(RESET)"
	@cd tests && bash test_cgi.sh
	@echo "$(GREEN)CGI tests completed!$(RESET)"

.PHONY: all clean fclean re test test-load test-memory test-edge test-static test-nginx test-quick test-cgi
