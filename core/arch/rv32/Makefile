
GCC_PREFIX ?= riscv-none-elf-

%.o : %.s
	$(GCC_PREFIX)gcc -march=rv32izicsr -c -o $@ $<

%.o : %.c
	$(GCC_PREFIX)gcc -march=rv32izicsr -c -o $@ $<

all : task_startup.o traps.o hal.o
	$(GCC_PREFIX)ld -r -n -T script.ld -o actinium.o hal.o traps.o

clean:
	rm -f *.o

