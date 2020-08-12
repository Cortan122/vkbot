LDFLAGS=-lcurl -lpthread -lm
CFLAGS=-g -Og -fdollars-in-identifiers
EXECUTABLES=print potatobot whoami concurrencytest send cronbot
# CC=gcc

all: $(EXECUTABLES) token.txt bottoken.txt apikey.txt
$(EXECUTABLES): botlib.o cJSON.o Buffer.o LinkedDict.o
botlib.o: cJSON.h botlib.h Buffer.h
Buffer.o: Buffer.h
LinkedDict.o: LinkedDict.h

run: all
	./$$(ls -t *.c | sed 's/.c$$//' | xargs -I R find R -prune 2>/dev/null | head -n 1)

clean:
	rm -f *.o $(EXECUTABLES) *.txt cJSON.*

%.txt:
	rm -f $@
	find .. -maxdepth 2 -name $@ | xargs -I R cp R .

cJSON.c: cJSON.h
	curl -O https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.c

cJSON.h:
	curl -O https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.h
