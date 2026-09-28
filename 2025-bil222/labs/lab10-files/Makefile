#TODO edit make file for your project
UNAME := $(shell uname -s)
ifeq ($(UNAME), Linux)
	CFLAGS = -lSDL2
endif
ifeq ($(UNAME), Darwin)
	CFLAGS = -F/Library/Frameworks -framework SDL2
endif

all: list.c view.c survivor.c controller.c drone.c map.c ai.c
	gcc *.c $(CFLAGS)

clean:
	rm -f *.o *.out