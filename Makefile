CC = g++
FLAGS = -g # -I$(LAZY)

SRC = main.cpp
LAZY = ./engines/lazy
LAZY_SRC = main.cpp $(wildcard $(LAZY)/*.cpp)
OPT_FLAGS = -g -O3

all: build

build:
	g++ $(FLAGS) $(SRC) -o main

lazy:
	g++ $(FLAGS) $(LAZY_SRC) -o lazy

lazy_opt:
	g++ $(OPT_FLAGS) $(LAZY_SRC) -o lazy

clean:
	rm -f ./lazy

