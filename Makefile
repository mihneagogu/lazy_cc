CC = g++
CC_VERSION = -std=c++17
FLAGS = $(CC_VERSION) -g -Wall # -I$(LAZY)
SRC = main.cpp

LAZY = ./engines/lazy
LAZY_SRC = $(wildcard $(LAZY)/*.cpp) lazy.cpp main.cpp
INCL_LAZY = 
ASAN = -fsanitize=address
OPT_FLAGS = $(CC_VERSION) -g -Wall -O3
LINKS = -pthread

# To check for leaks use valgrind, since gcc leak sanitizier seems to be broken...

all: build

reset: clean lazy

build:
	$(CC) $(FLAGS) $(SRC) -o main $(LINKS)

lazy:
	$(CC) $(INCL_LAZY) $(FLAGS) $(LAZY_SRC) -o lazy $(LINKS)

lk:
	$(CC) -E $(FLAGS) $(INCL_LAZY) $(LAZY_SRC) 

lazy_asan:
	$(CC) $(OPT_FLAGS) $(LAZY_SRC) $(ASAN) -o lazy_asan $(LINKS)

lazy_opt:
	$(CC) $(OPT_FLAGS) $(LAZY_SRC) -o lazy $(LINKS)

clean:
	rm -f ./lazy
	rm -f ./lazy_asan

