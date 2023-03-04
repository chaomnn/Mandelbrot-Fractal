.PHONY: all

CC = g++
CFLAGS = -Wall -Wextra

all: mandelbrot

mandelbrot: main.cpp
	$(CC) $(CFLAGS) -o $@ \
	$(shell pkg-config --cflags --libs glew) \
	$(shell pkg-config --cflags --libs sdl2) \
	$^
