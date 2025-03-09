
.PHONY: clean all

GCC_PREFIX ?= riscv-none-elf-

%.o : %.s
	$(GCC_PREFIX)gcc -march=rv32izicsr -c -o $@ $<

%.o : %.c
	$(GCC_PREFIX)gcc -O2 -ffreestanding -march=rv32izicsr -c -I ../.. -I ../../../magnesium -I . -o $@ $<

all : clean task_startup.o traps.o core.o pmp.o
	$(GCC_PREFIX)ld -r -n -o actinium.o core.o traps.o pmp.o

clean:
	rm -f *.o

