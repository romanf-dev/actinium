
.PHONY: clean all

%.o : %.c
	gcc -g -std=c11 -O2 -I . -I ../.. -I ../../../magnesium -c -o $@ $<

all : clean core.o
	gcc -r -o actinium.o core.o

clean:
	rm -f *.o

