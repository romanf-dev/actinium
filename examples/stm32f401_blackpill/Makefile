#
# By default this makefile builds image containing only tasks in C.
# To build C++ or Rust examples run 'make cpp_example' or 'make rust_example'
# before 'make all'. This substitutes the corresponding app with example in
# another language.
#

GCC_PREFIX ?= arm-none-eabi-
AC_PATH ?= ../..
PORT_PATH = $(AC_PATH)/arch/armv7m
INCLUDE_DIRS = -I $(PORT_PATH) -I $(AC_PATH) -I $(AC_PATH)/magnesium -I $(AC_PATH)/magnesium/nvic -I CMSIS

.DEFAULT_GOAL := all
.PHONY: all flash clean rust_example cpp_example kernel

%.o : %.c
	$(GCC_PREFIX)gcc -ffreestanding -mcpu=cortex-m4 -Wall $(INCLUDE_DIRS) -O2 -mthumb -I . -c -o $@ $<

%.o : $(PORT_PATH)/%.s
	$(GCC_PREFIX)gcc -mcpu=cortex-m4 -mthumb -c -o $@ $<

app%.rel : task%.c task_startup.o
	$(GCC_PREFIX)gcc -ffreestanding -mcpu=cortex-m4 -Wall -I $(AC_PATH)/usr/c -I CMSIS -O2 -mthumb -I . -c -o $@.o $<
	$(GCC_PREFIX)ld -nostdlib -r -n -T $(PORT_PATH)/task.ld -o $@ $@.o task_startup.o

%.o : %.s
	$(GCC_PREFIX)gcc -mcpu=cortex-m4 -mthumb -c -o $@ $<

kernel : main.o startup_stm32f401xc.o traps.o
	$(GCC_PREFIX)ld -nostdlib -r -n -T kernel.ld -o kernel.rel main.o startup_stm32f401xc.o traps.o

all : app0.rel app1.rel kernel
	$(AC_PATH)/ldgen.sh $(GCC_PREFIX)size kernel.rel app0.rel app1.rel > memory.ld
	$(GCC_PREFIX)objcopy --prefix-symbols t0 app0.rel blinker.task0
	$(GCC_PREFIX)objcopy --prefix-symbols t1 app1.rel sender.task1
	$(GCC_PREFIX)ld -n -o image.elf -T stm32f401.ld kernel.rel blinker.task0 sender.task1

clean :
	rm -f *.o *.task* *.elf *.rel memory.ld
	rm -rf rust_app0/target
	$(MAKE) --directory=cpp_app1 clean

rust_example : task_startup.o
	cd rust_app0 && cargo build --release
	mv rust_app0/target/thumbv7m-none-eabi/release/actinium_example rust_example.rel
	$(GCC_PREFIX)ld -nostdlib -r -n -T $(PORT_PATH)/task.ld -o app0.rel rust_example.rel task_startup.o

cpp_example : task_startup.o
	$(MAKE) --directory=cpp_app1
	$(GCC_PREFIX)ld -nostdlib -r -n -T $(PORT_PATH)/task.ld -o app1.rel cpp_app1/example.rel task_startup.o

flash : all
	openocd -f interface/stlink-v2.cfg -f target/stm32f4x.cfg -c "program image.elf verify reset exit"

