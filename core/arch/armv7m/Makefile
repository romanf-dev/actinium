
.PHONY: clean all

GCC_PREFIX ?= arm-none-eabi-

%.o : %.s
	$(GCC_PREFIX)gcc -mcpu=cortex-m4 -mthumb -c -o $@ $<

%.o : %.c
	$(GCC_PREFIX)gcc -mcpu=cortex-m4 -mthumb -ffreestanding -O2 -I . -I ../.. -I ../../../magnesium -I ../../../magnesium/nvic -c -o $@ $<

all : clean task_startup.o core.o actinium0.o
	$(GCC_PREFIX)ld -r -n -o actinium.o actinium0.o core.o

clean:
	rm -f *.o

