MAKEFLAGS += -j8
LDFLAGS=-lcurl -lpthread -lm
CFLAGS=-fdollars-in-identifiers -funsigned-char -Wall -Wextra -Wno-parentheses -Werror=vla
EXECUTABLES=print send cronbot deadlinebot botbot gapi vkapi tgrevbot
# CC=gcc

EXTRALIBS=potatobot.o etcbot.o extralib.o
HEADERS=$(wildcard *.h) cJSON.h
LIBS=$(filter-out $(EXTRALIBS),$(patsubst %.h,%.o,$(HEADERS)))

ifdef NO_DEBUG
  CFLAGS += -O3 -DNO_DEBUG -Wno-unused-value -Wno-unused-label -fdata-sections -ffunction-sections
  LDFLAGS += -Wl,--gc-sections
else
  CFLAGS += -g -Og
endif

ifneq ($(CC), tcc)
  EXECUTABLES += potatobot
endif

all: $(EXECUTABLES) token.txt bottoken.txt apikey.txt pybottoken.txt tgtoken.txt
$(EXECUTABLES): $(LIBS)
$(LIBS) $(EXTRALIBS): $(HEADERS)
$(patsubst %,%.o,$(EXECUTABLES)): $(HEADERS)
botbot: $(EXTRALIBS)
potatobot send tgrevbot: extralib.o

run: all
	./$$(ls -t *.c | sed 's/.c$$//' | xargs -I R find R -prune 2>/dev/null | head -n 1)

clean:
	rm -f *.o $(EXECUTABLES) potatobot *.txt cJSON.*

%.txt:
	rm -f $@
	find .. -maxdepth 2 -name $@ | xargs -I R cp R .

cJSON.c: cJSON.h
	curl --silent -O https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.c

cJSON.h:
	curl --silent -O https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.h
