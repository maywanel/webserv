NAME      = webserv

CXX       = c++
CXXFLAGS  = -g -Wall -Wextra -Werror -std=c++98 -I includes

SRCDIR    = src
OBJDIR    = obj

SRCS      = $(wildcard $(SRCDIR)/*.cpp)
OBJS      = $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SRCS))

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJS)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
