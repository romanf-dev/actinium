GCC_PREFIX ?= arm-none-eabi-
AC_DIR = ../../..

.DEFAULT_GOAL := all

%.o : %.cpp
	$(GCC_PREFIX)g++ -std=c++20 -fno-rtti -fno-exceptions -ffreestanding -mcpu=cortex-m3 -Wall -O2 -mthumb -I $(AC_DIR)/usr/cpp -c -o $@ $<

all : cpp_example.o
	$(GCC_PREFIX)g++ -Wl,-r -Wl,-n -Wl,--gc-sections -mthumb -Wl,-T,$(AC_DIR)/usr/cpp/prelink.ld -o example.rel cpp_example.o

clean:
	rm -f *.o *.rel

