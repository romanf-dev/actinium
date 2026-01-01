#
# Simple makefile for compiling all .c in the current folder.
#

GCC_PREFIX ?= arm-none-eabi-
SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,%.o,$(SRCS))

.DEFAULT_GOAL := all
.PHONY: all clean

%.o : %.c
	$(GCC_PREFIX)gcc -ffreestanding -mcpu=cortex-m4 -std=gnu11 -DSTM32F401xC -c -I.. -I../../../usr/c -I. -I../CMSIS -O2 -mthumb -o $@ $<

all : clean $(OBJS)
	$(GCC_PREFIX)ld -n -r -nostdlib -o usb.rel $(OBJS)

clean :
	rm -f *.o *.rel

