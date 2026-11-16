# Makefile (given). Each program is built from every .c file in its own folder, plus chan/ (your
# channel API) and common/ (code you share between programs, if any). Add files freely; change
# this file if you need to (flags, libraries).
#
#   make              build bin/center bin/captain bin/performer bin/viewer bin/swarmctl
#   make test         the channel API tests, every scheme (tests/test_chan)
#   make test2        only the Phase 2 schemes: local shm file
#   make test3        only the Phase 3 schemes: tcp udp
#   make check2       Phase 2 shows, black-box (tests/check.sh phase2)
#   make check3       Phase 3 shows, black-box (tests/check.sh phase3)
#   make clean

CC       ?= cc
CFLAGS   ?= -std=gnu11 -Wall -Wextra -O2 -g
CPPFLAGS += -Iinclude -Icommon -Ichan
LDLIBS   += -pthread -lrt -lm

# the viewer gets SDL2 when it is installed (apt install libsdl2-dev): HAVE_SDL2 tells your code
SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS   := $(shell pkg-config --libs sdl2 2>/dev/null)
ifneq ($(SDL_LIBS),)
viewer_FLAGS := -DHAVE_SDL2 $(SDL_CFLAGS)
viewer_LIBS  := $(SDL_LIBS)
endif

PROGRAMS := center captain performer viewer swarmctl
SHARED   := $(wildcard chan/*.c) $(wildcard common/*.c)
HEADERS  := $(wildcard include/*.h chan/*.h common/*.h)

all: $(addprefix bin/,$(PROGRAMS))

define program
bin/$(1): $$(wildcard $(1)/*.c) $$(SHARED) $$(HEADERS) $$(wildcard $(1)/*.h) | bin
	$$(CC) $$(CPPFLAGS) -I$(1) $$(CFLAGS) $$($(1)_FLAGS) -o $$@ $$(wildcard $(1)/*.c) $$(SHARED) $$(LDLIBS) $$($(1)_LIBS)
endef
$(foreach p,$(PROGRAMS),$(eval $(call program,$(p))))

bin/test_chan: tests/test_chan.c $(SHARED) $(HEADERS) | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_chan.c $(SHARED) $(LDLIBS)

bin/framecheck: tests/framecheck.c include/swarm.h | bin
	$(CC) -Iinclude $(CFLAGS) -o $@ tests/framecheck.c

bin:
	mkdir -p bin

test: bin/test_chan
	./bin/test_chan

test2: bin/test_chan
	./bin/test_chan local shm file

test3: bin/test_chan
	./bin/test_chan tcp udp

check2: all bin/framecheck
	tests/check.sh phase2

check3: all bin/framecheck
	tests/check.sh phase3

clean:
	$(RM) -r bin tests/logs

.PHONY: all test test2 test3 check2 check3 clean
