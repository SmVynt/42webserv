NAME		= webserv

CXX			= c++
CXXFLAGS	= -Wall -Wextra -Werror -std=c++17
RM			= rm -rf

SRCS_DIR	= srcs
OBJS_DIR	= objs
INCS_DIR	= includes

SRCS		= main.cpp \
			  Server.cpp \
			  Config.cpp \
			  Request.cpp \
			  Response.cpp \
			  Client.cpp \
			  utils.cpp

OBJS		= $(addprefix $(OBJS_DIR)/, $(SRCS:.cpp=.o))

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

$(OBJS_DIR)/%.o: $(SRCS_DIR)/%.cpp
	@mkdir -p $(OBJS_DIR)
	$(CXX) $(CXXFLAGS) -I$(INCS_DIR) -c $< -o $@

clean:
	$(RM) $(OBJS_DIR)

fclean: clean
	$(RM) $(NAME)

re: fclean all

.PHONY: all clean fclean re
