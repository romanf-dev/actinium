#
# Simepl makefile for compiling all .c and .s files in the current folder.
# No dependency tracking, use make clean if a header is changed.
#

SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,%.o,$(SRCS))

GCC_PREFIX ?= arm-none-eabi-

%.o : %.c
	arm-none-eabi-gcc -ffreestanding -mcpu=cortex-m4 -std=gnu11 -DSTM32F401xC -c -I.. -I../../../usr/c -I. -I../CMSIS -O2 -mthumb -o $@ $<

%.o : %.s
	arm-none-eabi-gcc -mcpu=cortex-m4 -c -x assembler-with-cpp -mthumb -o $@ $<

all : clean $(OBJS)
	arm-none-eabi-ld -r -nostdlib -o app.task $(OBJS)

clean :
	rm -f *.o *.task

