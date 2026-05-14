CC         = gcc
CFLAGS     = -Wall -Wextra -std=c99 -I..
SDL_CFLAGS = $(shell sdl2-config --cflags)
SDL_LIBS   = $(shell sdl2-config --libs)

.PHONY: all test clean

all: pixel_wall

pixel_wall: main.c display_buf.c display_sim.c input_sim.c eeprom_sim.c \
            ../conway.c ../stacker.c ../space_invaders.c
	$(CC) $(CFLAGS) $(SDL_CFLAGS) $^ -o $@ $(SDL_LIBS)

test_display: test_display.c display_buf.c
	$(CC) $(CFLAGS) $^ -o $@

test_input: test_input.c input_sim.c
	$(CC) $(CFLAGS) $(SDL_CFLAGS) $^ -o $@ $(SDL_LIBS)

test_conway: test_conway.c ../conway.c display_buf.c
	$(CC) $(CFLAGS) $^ -o $@

test_stacker: test_stacker.c ../stacker.c display_buf.c eeprom_sim.c
	$(CC) $(CFLAGS) $^ -o $@

test_space_invaders: test_space_invaders.c ../space_invaders.c display_buf.c eeprom_sim.c
	$(CC) $(CFLAGS) $^ -o $@

test: test_display test_input test_conway test_stacker test_space_invaders
	./test_display
	./test_input
	./test_conway
	./test_stacker
	./test_space_invaders

clean:
	rm -f pixel_wall test_display test_input test_conway test_stacker \
	      test_space_invaders
