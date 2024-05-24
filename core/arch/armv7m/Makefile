
GCC_PREFIX ?= arm-none-eabi-

%.o : %.s
	$(GCC_PREFIX)gcc -mcpu=cortex-m4 -mthumb -c -o $@ $<

all : task_startup.o actinium.o

clean:
	rm -f *.o

