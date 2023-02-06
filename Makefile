CC = g++
FLAGS = -g -Wall # -I$(LAZY)

SRC = main.cpp
LAZY = ./engines/lazy
LAZY_SRC = main.cpp lazy.cpp $(wildcard $(LAZY)/*.cpp)
ASAN = -fsanitize=address
OPT_FLAGS = -g -Wall -O3

# To check for leaks use valgrind, since gcc leak sanitizier seems to be broken...

all: build

build:
	g++ $(FLAGS) $(SRC) -o main

lazy:
	g++ $(FLAGS) $(LAZY_SRC) -o lazy

lazy_asan:
	g++ $(OPT_FLAGS) $(LAZY_SRC) $(ASAN) -o lazy_asan 

lazy_opt:
	g++ $(OPT_FLAGS) $(LAZY_SRC) -o lazy

clean:
	rm -f ./lazy
	rm -f ./lazy_asan

