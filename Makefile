NAME = ubircast

LDLIBS += `sdl-config --libs`
CFLAGS += `sdl-config --cflags` -Wall -Wextra -Wstrict-prototypes -O2 -ffast-math -std=c99

all: $(NAME)

$(NAME): $(NAME).o

clean:
	$(RM) $(NAME).o

distclean: clean
	$(RM) $(NAME)

re: distclean all
