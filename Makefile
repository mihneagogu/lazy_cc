CC = g++
FLAGS = -g
SRC = main.cpp
LAZY_SRC = main.cpp engines/lazy/table.cpp engines/lazy/lazy_engine.cpp
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

