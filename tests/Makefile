
.PHONY: all clean fwk tasks kernel

%.o : %.c
	gcc -pedantic -std=c11 -Wall -O2 -g -Wall -I ../core -I ../core/arch/test -I ../magnesium -I . -c -o $@ $<

all : fwk test0.o
	gcc -o test test0.o ../core/arch/test/actinium.o

fwk : 
	$(MAKE) --directory=../core/arch/test

clean :
	rm -f *.o test 
	$(MAKE) --directory=../core/arch/test clean

