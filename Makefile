GREEN			= \033[0;32m
YELLOW			= \033[0;33m
RESET			= \033[0m

NAME			= webserv
CXX				= c++
CXXFLAGS		= -Wall -Wextra -Werror -std=c++17
OBJ_DIR			= obj/
INC_DIR			= includes/
SRC_DIR			= srcs/

CORE_SRCS	= main.cpp \
				core/Server.cpp \
				core/Client.cpp

PARSER_SRCS	= parser/Config.cpp \
				parser/Request.cpp \
				parser/parserUtils.cpp

LOGIC_SRCS	= logic/Response.cpp \
				logic/Methods.cpp

CGI_SRCS	= cgi/CGI.cpp

SRCS		= $(CORE_SRCS) $(PARSER_SRCS) $(LOGIC_SRCS) $(CGI_SRCS) utils.cpp

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

.PHONY: all clean fclean re