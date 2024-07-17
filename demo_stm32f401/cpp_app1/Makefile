#
# Simple makefile for compiling all .cpp files in the current folder.
# No dependency tracking, use make clean if a header is changed.
#

SRCS = $(wildcard *.cpp)
OBJS = $(patsubst %.cpp,%.o,$(SRCS))

GCC_PREFIX ?= arm-none-eabi-

%.o : %.cpp
	$(GCC_PREFIX)g++ -std=c++20 -fno-rtti -fno-exceptions -ffreestanding -mcpu=cortex-m3 -Wall -O2 -mthumb -I ../../core/cpp_task -c -o $@ $<

all : $(OBJS)
	$(GCC_PREFIX)g++ -Wl,-r -Wl,-n -Wl,--gc-sections -mthumb -Wl,-T,../../core/cpp_task/prelink.ld -o example.o $(OBJS)

clean:
	rm -f *.o

