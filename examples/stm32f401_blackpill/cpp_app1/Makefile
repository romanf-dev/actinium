
GCC_PREFIX ?= arm-none-eabi-

%.o : %.cpp
	$(GCC_PREFIX)g++ -std=c++20 -fno-rtti -fno-exceptions -ffreestanding -mcpu=cortex-m3 -Wall -O2 -mthumb -I ../../../usr/cpp -c -o $@ $<

all : cpp_example.o
	$(GCC_PREFIX)g++ -Wl,-r -Wl,-n -Wl,--gc-sections -mthumb -Wl,-T,../../../usr/cpp/prelink.ld -o example.o cpp_example.o
	$(GCC_PREFIX)ld -nostdlib -r -n -T ../../../arch/armv7m/task.ld -o app.task example.o ../task_startup.o

clean:
	rm -f *.o

