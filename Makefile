CC = g++
CC_VERSION = -std=c++17
FLAGS = $(CC_VERSION) -g -Wall # -I$(LAZY)
SRC = main.cpp

LAZY = ./engines/lazy
LAZY_SRC = $(wildcard $(LAZY)/*.cpp) lazy.cpp main.cpp
ASAN = -fsanitize=address
TSAN = -fsanitize=thread
OPT_FLAGS = $(CC_VERSION) -g -Wall -O3
LINKS = -pthread

reset: clean lazy

lazy:
	$(CC) $(FLAGS) $(LAZY_SRC) -o lazy $(LINKS)

lazy_asan:
	$(CC) $(FLAGS) $(LAZY_SRC) $(ASAN) -o lazy_asan $(LINKS)

lazy_tsan:
	$(CC) $(FLAGS) $(LAZY_SRC) $(TSAN) -o lazy_tsan $(LINKS)

all_lazy: lazy lazy_asan lazy_tsan

lazy_opt:
	$(CC) $(OPT_FLAGS) $(LAZY_SRC) -o lazy $(LINKS)

clean:
	rm -f ./lazy
	rm -f ./lazy_asan
	rm -f ./lazy_tsan

