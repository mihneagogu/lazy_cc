CC = g++
CC_VERSION = -std=c++17
FLAGS = $(CC_VERSION) -g -Wall 
OPT_FLAGS = $(CC_VERSION) -g -Wall -O3
SRC = main.cpp

LAZY = ./engines/lazy
LAZY_SRC = $(wildcard $(LAZY)/*.cpp) lazy.cpp main.cpp
ASAN = -fsanitize=address
TSAN = -fsanitize=thread
UBSAN = -fsanitize=undefined
LINKS = -pthread

OUTPUTS = ./lazy ./lazy_asan ./lazy_tsan ./lazy_opt ./lazy_asan_opt ./lazy_tsan_opt

reset: clean lazy

all_lazy: lazy lazy_asan lazy_tsan

all_lazy_opt: lazy_opt lazy_asan_opt lazy_tsan_opt lazy_ubsan_opt

lazy:
	$(CC) $(FLAGS) $(LAZY_SRC) -o lazy $(LINKS)

lazy_asan:
	$(CC) $(FLAGS) $(LAZY_SRC) $(ASAN) -o lazy_asan $(LINKS)

lazy_tsan:
	$(CC) $(FLAGS) $(LAZY_SRC) $(TSAN) -o lazy_tsan $(LINKS)

lazy_opt:
	$(CC) $(OPT_FLAGS) $(LAZY_SRC) -o lazy_opt $(LINKS)

lazy_asan_opt:
	$(CC) $(OPT_FLAGS) $(LAZY_SRC) $(ASAN) -o lazy_asan_opt $(LINKS)

lazy_tsan_opt:
	$(CC) $(OPT_FLAGS) $(LAZY_SRC) $(TSAN) -o lazy_tsan_opt $(LINKS)

lazy_ubsan_opt:
	$(CC) $(OPT_FLAGS) $(LAZY_SRC) $(UBSAN) -o lazy_ubsan_opt $(LINKS)

clean:
	rm -f ./lazy
	rm -f ./lazy_asan
	rm -f ./lazy_tsan
	rm -f ./lazy_opt
	rm -f ./lazy_asan_opt
	rm -f ./lazy_tsan_opt
	rm -f ./lazy_ubsan_opt

