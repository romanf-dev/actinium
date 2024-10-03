
%.o : %.c
	gcc -g -O2 -I ../rv32 -I ../.. -I ../../../magnesium -c -o $@ $<

all : hal.o
	gcc -r -o actinium.o hal.o

clean:
	rm -f *.o

