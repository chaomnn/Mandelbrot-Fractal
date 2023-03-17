.PHONY: all

CC = g++
CFLAGS = -Wall -Wextra

all: mandelbrot

%-shader.o: %.shader
	ld -r -b binary -o $@ $^

mandelbrot: main.cpp vertex-shader.o mandelbrot-shader.o
	$(CC) $(CFLAGS) -o $@ \
	$(shell pkg-config --cflags --libs glew) \
	$(shell pkg-config --cflags --libs sdl2) \
	$^
