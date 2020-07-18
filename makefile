LDFLAGS=-lcurl -lpthread

all: print potatobot whoami token.txt bottoken.txt
print potatobot whoami: botlib.o cJSON.o Buffer.o
botlib.o: cJSON.h botlib.h Buffer.h
Buffer.o: Buffer.h

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
