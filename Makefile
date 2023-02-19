CC = g++
CC_VERSION = -std=c++17
FLAGS = $(CC_VERSION) -g -Wall # -I$(LAZY)
SRC = main.cpp

LAZY = ./engines/lazy
LAZY_SRC = main.cpp lazy.cpp $(wildcard $(LAZY)/*.cpp)
ASAN = -fsanitize=address
OPT_FLAGS = $(CC_VERSION) -g -Wall -O3

# To check for leaks use valgrind, since gcc leak sanitizier seems to be broken...

all: build

build:
	$(CC) $(FLAGS) $(SRC) -o main

lazy:
	$(CC) $(FLAGS) $(LAZY_SRC) -o lazy

lazy_asan:
	$(CC) $(OPT_FLAGS) $(LAZY_SRC) $(ASAN) -o lazy_asan 

lazy_opt:
	$(CC) $(OPT_FLAGS) $(LAZY_SRC) -o lazy

clean:
	rm -f ./lazy
	rm -f ./lazy_asan

