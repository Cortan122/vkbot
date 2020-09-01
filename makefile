MAKEFLAGS += -j8
LDFLAGS=-lcurl -lpthread -lm
CFLAGS=-fdollars-in-identifiers -funsigned-char -Wall -Wextra -Wno-parentheses
EXECUTABLES=print potatobot whoami concurrencytest send cronbot deadlinebot botbot
# CC=gcc

HEADERS=$(wildcard *.h) cJSON.h
LIBS=$(patsubst %.h,%.o,$(HEADERS))

ifdef NO_DEBUG
  CFLAGS += -O3 -DNO_DEBUG -Wno-unused-value -Wno-unused-label
else
  CFLAGS += -g -Og
endif

all: $(EXECUTABLES) token.txt bottoken.txt apikey.txt
$(EXECUTABLES): $(LIBS)
$(LIBS): $(HEADERS)
$(patsubst %,%.o,$(EXECUTABLES)): $(HEADERS)
botbot: potatobot.o
# botbot: CC=gcc # tcc does not support __attribute__((weak))

run: all
	./$$(ls -t *.c | sed 's/.c$$//' | xargs -I R find R -prune 2>/dev/null | head -n 1)

clean:
	rm -f *.o $(EXECUTABLES) *.txt cJSON.*

%.txt:
	rm -f $@
	find .. -maxdepth 2 -name $@ | xargs -I R cp R .

cJSON.c: cJSON.h
	curl --silent -O https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.c

cJSON.h:
	curl --silent -O https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.h
