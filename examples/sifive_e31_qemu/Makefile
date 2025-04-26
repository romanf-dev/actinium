
GCC_PREFIX ?= riscv32-unknown-elf-
AC_PATH ?= ../..
PORT_PATH = $(AC_PATH)/arch/rv32
INCLUDE_DIRS = -I $(PORT_PATH) -I $(AC_PATH) -I $(AC_PATH)/magnesium

.PHONY: all clean kernel

%.o: %.c
	$(GCC_PREFIX)gcc -ffreestanding -Wall -O2 -march=rv32izicsr $(INCLUDE_DIRS) -I . -c -o $@ $<

app%.task: task%.c task_startup.o
	$(GCC_PREFIX)gcc -ffreestanding -Wall -O2 -march=rv32i -I $(AC_PATH)/usr/c -I . -c -o $@.o $<
	$(GCC_PREFIX)ld -nostdlib -r -n -T $(PORT_PATH)/task.ld -o $@ $@.o task_startup.o

%.o : $(PORT_PATH)/%.s
	$(GCC_PREFIX)gcc -march=rv32izicsr -c -o $@ $<

%.o: %.s
	$(GCC_PREFIX)gcc -march=rv32izicsr -c -o $@ $<

all: clean kernel app0.task app1.task
	$(AC_PATH)/ldgen.sh 0x20400000 0x80000000 $(GCC_PREFIX)
	$(GCC_PREFIX)ld -n -o image.elf -T ldscript.ld

kernel: main.o port.o startup.o traps.o
	$(GCC_PREFIX)ld -nostdlib -r -n -T kernel.ld -o kernel.0 main.o port.o startup.o traps.o

clean:
	rm -f *.o *.elf *.0 *.pfx *.task ldscript.ld

