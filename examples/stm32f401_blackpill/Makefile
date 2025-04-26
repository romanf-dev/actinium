
GCC_PREFIX ?= arm-none-eabi-
AC_PATH ?= ../..
PORT_PATH = $(AC_PATH)/arch/armv7m
INCLUDE_DIRS = -I $(PORT_PATH) -I $(AC_PATH) -I $(AC_PATH)/magnesium -I $(AC_PATH)/magnesium/nvic

.PHONY: all clean rust_example cpp_example tasks kernel

%.o : %.c
	$(GCC_PREFIX)gcc -ffreestanding -mcpu=cortex-m4 -Wall $(INCLUDE_DIRS) -O2 -mthumb -I . -c -o $@ $<

%.o : $(PORT_PATH)/%.s
	$(GCC_PREFIX)gcc -mcpu=cortex-m4 -mthumb -c -o $@ $<

app%.task : task%.c task_startup.o
	$(GCC_PREFIX)gcc -ffreestanding -mcpu=cortex-m4 -Wall -I $(AC_PATH)/usr/c -O2 -mthumb -I . -c -o $@.o $<
	$(GCC_PREFIX)ld -nostdlib -r -n -T $(PORT_PATH)/task.ld -o $@ $@.o task_startup.o

%.o : %.s
	$(GCC_PREFIX)gcc -mcpu=cortex-m4 -mthumb -c -o $@ $<

all : app0.task app1.task kernel
	$(AC_PATH)/ldgen.sh 0x08000000 0x20000000 $(GCC_PREFIX)
	$(GCC_PREFIX)ld -n -o image.elf -T ldscript.ld

kernel : main.o exceptions.o startup_stm32f401xc.o traps.o
	$(GCC_PREFIX)ld -nostdlib -r -n -T LinkerScript.ld -o kernel.0 main.o exceptions.o startup_stm32f401xc.o traps.o

clean :
	rm -f *.o *.task *.elf *.0 *.pfx ldscript.ld
	$(MAKE) --directory=rust_app0 clean
	$(MAKE) --directory=cpp_app1 clean

rust_example : task_startup.o
	$(MAKE) --directory=rust_app0
	mv rust_app0/app.task app0.task

cpp_example : task_startup.o
	$(MAKE) --directory=cpp_app1
	mv cpp_app1/app.task app1.task

