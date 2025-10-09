# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::   #
#    Makefile                                           :+:      :+:    :+:   #
#                                                     +:+ +:+         +:+     #
#    By: webserv                                    +#+  +:+       +#+        #
#                                                 +#+#+#+#+#+   +#+           #
#    Created: 2025/07/22 00:00:00 by webserv          #+#    #+#             #
#    Updated: 2025/07/22 00:00:00 by webserv         ###   ########.fr       #
#                                                                              #
# **************************************************************************** #

NAME		= webserv

CXX			= c++
CXXFLAGS	= -Wall -Wextra -Werror -std=c++98 -g

SRCDIR		= src
INCDIR		= include
OBJDIR		= obj

SOURCES		= main.cpp \
			  Server.cpp \
			  Client.cpp \
			  Request.cpp \
			  Response.cpp \
			  Config.cpp \
			  Location.cpp \
			  Utils.cpp \
			  CGI.cpp \
			  Logger.cpp \
			  Cookie.cpp \
			  Session.cpp \
			  Compression.cpp \
			  Range.cpp

HEADERS		= Server.hpp \
			  Client.hpp \
			  Request.hpp \
			  Response.hpp \
			  Config.hpp \
			  Location.hpp \
			  Utils.hpp \
			  CGI.hpp \
			  Logger.hpp \
			  Cookie.hpp \
			  Session.hpp \
			  Compression.hpp \
			  Range.hpp \
			  webserv.hpp

SRCS		= $(addprefix $(SRCDIR)/, $(SOURCES))
INCS		= $(addprefix $(INCDIR)/, $(HEADERS))
OBJS		= $(addprefix $(OBJDIR)/, $(SOURCES:.cpp=.o))

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME) -lz

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp $(INCS) | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -I$(INCDIR) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
