CC         = gcc
CFLAGS     = -Wall -Wextra -std=c99 -I..
SDL_CFLAGS = $(shell sdl2-config --cflags)
SDL_LIBS   = $(shell sdl2-config --libs)

.PHONY: all test clean

all: pixel_wall

pixel_wall: main.c display_buf.c display_sim.c
	$(CC) $(CFLAGS) $(SDL_CFLAGS) $^ -o $@ $(SDL_LIBS)

test_display: test_display.c display_buf.c
	$(CC) $(CFLAGS) $^ -o $@

test: test_display
	./test_display

clean:
	rm -f pixel_wall test_display
