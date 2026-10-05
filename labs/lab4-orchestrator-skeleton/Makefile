CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -std=gnu11 -g -Iinclude

all: aiorch agents

aiorch: src/main.c src/orchestrator.c include/orchestrator.h
	$(CC) $(CFLAGS) -o aiorch src/main.c src/orchestrator.c

agents: sleeper failer wordcount

sleeper: agents/sleeper.c
	$(CC) $(CFLAGS) -o agents/sleeper agents/sleeper.c

failer: agents/failer.c
	$(CC) $(CFLAGS) -o agents/failer agents/failer.c

wordcount: agents/wordcount.c
	$(CC) $(CFLAGS) -o agents/wordcount agents/wordcount.c

clean:
	rm -f aiorch agents/sleeper agents/failer agents/wordcount
	rm -f logs/*.log reports/*.report status/*.status status/*.tmp

.PHONY: all agents clean
