LDFLAGS=-lcurl -lpthread
CFLAGS=-g -O3
EXECUTABLES=print potatobot whoami concurrencytest
CC=gcc

all: $(EXECUTABLES) token.txt bottoken.txt
$(EXECUTABLES): botlib.o cJSON.o Buffer.o LinkedDict.o
botlib.o: cJSON.h botlib.h Buffer.h
Buffer.o: Buffer.h
LinkedDict.o: LinkedDict.h

run: all
	./$$(ls -t *.c | sed 's/.c$$//' | xargs -I R find R -prune 2>/dev/null | head -n 1)

clean:
	rm -f *.o print whoami potatobot *.txt cJSON.*

%.txt:
	rm -f $@
	find .. -maxdepth 2 -name $@ | xargs -I R cp R .

cJSON.c: cJSON.h
	curl -O https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.c

cJSON.h:
	curl -O https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.h
