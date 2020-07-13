LDFLAGS=-lcurl -lcjson -lpthread

all: print potatobot whoami
print: botlib.o
whoami: botlib.o token.txt
potatobot: botlib.o token.txt bottoken.txt

run: all
	./$$(ls -t $$(ls *.c | sed 's/.c$$//') 2>/dev/null | head -n 1)

clean:
	rm -f *.o print whoami potatobot *.txt cJSON.*

%.txt:
	rm $<
	find .. -maxdepth 2 -name $< | xargs -I R cp R .

cJSON.c: cJSON.h
	curl -O https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.c

cJSON.h:
	curl -O https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.h
