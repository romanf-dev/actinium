
GCC_PREFIX ?= riscv-none-elf-

.PHONY: all clean fwk tasks kernel

%.o : %.c
	$(GCC_PREFIX)gcc -ffreestanding -Wall -O2 -march=rv32izicsr -I ../core -I ../core/arch/rv32 -I ../magnesium -I . -c -o $@ $<

task%.o : task%.c
	$(GCC_PREFIX)gcc -ffreestanding -Wall -O2 -march=rv32i -I ../core/c_task -I . -c -o $@ $<

%.o : %.s
	$(GCC_PREFIX)gcc -march=rv32izicsr -c -o $@ $<

all : clean tasks kernel
	rm main.o startup.o task0.o task1.o
	../core/ldgen.sh 0x20400000 0x80000000 $(GCC_PREFIX)
	$(GCC_PREFIX)ld -n -o image.elf -T ldscript.ld

tasks : task0.o task1.o fwk
	$(GCC_PREFIX)ld -nostdlib -r -n -T ../core/arch/rv32/task.ld -o app0.o task0.o ../core/arch/rv32/task_startup.o
	$(GCC_PREFIX)ld -nostdlib -r -n -T ../core/arch/rv32/task.ld -o app1.o task1.o ../core/arch/rv32/task_startup.o

kernel : main.o startup.o fwk   
	$(GCC_PREFIX)ld -nostdlib -r -n -T kernel.ld -o kernel.0 main.o startup.o ../core/arch/rv32/actinium.o

fwk: 
	$(MAKE) --directory=../core/arch/rv32

clean:
	rm -f *.o *.elf *.0 *.pfx ldscript.ld
	$(MAKE) --directory=../core/arch/rv32 clean

