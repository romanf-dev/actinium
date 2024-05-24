
GCC_PREFIX ?= arm-none-eabi-

.PHONY: all clean fwk tasks kernel

%.o : %.c
	$(GCC_PREFIX)gcc -pedantic -ffreestanding -mcpu=cortex-m4 -DMG_NVIC_PRIO_BITS=4 -Wall -I ../core -I ../core/arch/armv7m -I ../magnesium -I ../magnesium/nvic -O2 -mthumb -I . -c -o $@ $<

%.o : %.s
	$(GCC_PREFIX)gcc -mcpu=cortex-m4 -mthumb -c -o $@ $<

all : tasks kernel
	rm main.o startup_stm32f401xc.o task0.o task1.o
	../core/ldgen.sh 0x08000000 0x20000000

tasks : task0.o task1.o fwk
	$(GCC_PREFIX)ld -nostdlib -r -n -T ../core/task.ld -o app0.o task0.o ../core/arch/armv7m/task_startup.o
	$(GCC_PREFIX)ld -nostdlib -r -n -T ../core/task.ld -o app1.o task1.o ../core/arch/armv7m/task_startup.o

kernel : main.o startup_stm32f401xc.o fwk   
	$(GCC_PREFIX)ld -nostdlib -r -n -T LinkerScript.ld -o kernel.0 main.o startup_stm32f401xc.o ../core/arch/armv7m/actinium.o

fwk: 
	$(MAKE) --directory=../core/arch/armv7m

clean:
	rm -f *.o *.elf *.0 ldscript.ld
	$(MAKE) --directory=../core/arch/armv7m clean

