CC = g++
FLAGS = -g
SRC = main.cpp
OPT_FLAGS = -O3

all: build

build:
	g++ $(FLAGS) $(SRC) -o main

clean:
	rm -f ./main

