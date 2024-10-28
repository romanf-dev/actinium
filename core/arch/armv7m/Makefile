
GCC_PREFIX ?= arm-none-eabi-

%.o : %.s
	$(GCC_PREFIX)gcc -mcpu=cortex-m4 -mthumb -c -o $@ $<

%.o : %.c
	$(GCC_PREFIX)gcc -mcpu=cortex-m4 -mthumb -DMG_NVIC_PRIO_BITS=4 -ffreestanding -O2 -I . -I ../.. -I ../../../magnesium -I ../../../magnesium/nvic -c -o $@ $<

all : task_startup.o core.o actinium0.o
	$(GCC_PREFIX)ld -r -n -o actinium.o actinium0.o core.o

clean:
	rm -f *.o

