
GCC_PREFIX ?= arm-none-eabi-

.PHONY: all clean fwk tasks kernel

%.o : %.c
	$(GCC_PREFIX)gcc -pedantic -ffreestanding -mcpu=cortex-m4 -DMG_NVIC_PRIO_BITS=4 -Wall -I ../core -I ../core/arch/armv7m -I ../magnesium -I ../magnesium/nvic -O2 -mthumb -I . -c -o $@ $<

task%.o : task%.c
	$(GCC_PREFIX)gcc -pedantic -ffreestanding -mcpu=cortex-m4 -Wall -DAC_TASK -I ../core -I ../core/arch/armv7m -O2 -mthumb -I . -c -o $@ $<

%.o : %.s
	$(GCC_PREFIX)gcc -mcpu=cortex-m4 -mthumb -c -o $@ $<

all : tasks kernel
	rm main.o startup_stm32f401xc.o task0.o task1.o
	../core/ldgen.sh 0x08000000 0x20000000 arm-none-eabi-
	arm-none-eabi-ld -n -o image.elf -T ldscript.ld

tasks : task0.o task1.o fwk
	$(GCC_PREFIX)ld -nostdlib -r -n -T ../core/arch/armv7m/task.ld -o app0.o task0.o ../core/arch/armv7m/task_startup.o
	$(GCC_PREFIX)ld -nostdlib -r -n -T ../core/arch/armv7m/task.ld -o app1.o task1.o ../core/arch/armv7m/task_startup.o

kernel : main.o startup_stm32f401xc.o fwk   
	$(GCC_PREFIX)ld -nostdlib -r -n -T LinkerScript.ld -o kernel.0 main.o startup_stm32f401xc.o ../core/arch/armv7m/actinium.o

fwk: 
	$(MAKE) --directory=../core/arch/armv7m

clean:
	rm -f *.o *.elf *.0 *.pfx ldscript.ld
	$(MAKE) --directory=../core/arch/armv7m clean
	$(MAKE) --directory=rust_app0 clean
	$(MAKE) --directory=cpp_app1 clean

rust_example: fwk
	$(MAKE) --directory=rust_app0
	mv rust_app0/target/thumbv7m-none-eabi/release/actinium_example task0.o

cpp_example: fwk
	$(MAKE) --directory=cpp_app1
	mv cpp_app1/example.o task1.o

