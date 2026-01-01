GCC_PREFIX ?= riscv32-unknown-elf-
AC_PATH ?= ../..
PORT_PATH = $(AC_PATH)/arch/rv32
INCLUDE_DIRS = -I $(PORT_PATH) -I $(AC_PATH) -I $(AC_PATH)/magnesium

.PHONY: all clean kernel
.DEFAULT_GOAL := all

%.o: %.c
	$(GCC_PREFIX)gcc -ffreestanding -Wall -O2 -march=rv32izicsr $(INCLUDE_DIRS) -I . -c -o $@ $<

%.o: $(PORT_PATH)/%.s
	$(GCC_PREFIX)gcc -march=rv32izicsr -c -o $@ $<

%.o: %.s
	$(GCC_PREFIX)gcc -march=rv32izicsr -c -o $@ $<

app%.rel: task%.c task_startup.o
	$(GCC_PREFIX)gcc -ffreestanding -Wall -O2 -march=rv32i -I $(AC_PATH)/usr/c -I . -c -o $@.o $<
	$(GCC_PREFIX)ld -nostdlib -r -n -T $(PORT_PATH)/task.ld -o $@ $@.o task_startup.o

kernel: main.o port.o startup.o traps.o
	$(GCC_PREFIX)ld -nostdlib -r -n -T kernel.ld -o kernel.rel main.o port.o startup.o traps.o

all: clean kernel app0.rel app1.rel
	$(AC_PATH)/ldgen.sh $(GCC_PREFIX)size kernel.rel app0.rel app1.rel > memory.ld
	$(GCC_PREFIX)objcopy --prefix-symbols t0 app0.rel writer.task0
	$(GCC_PREFIX)objcopy --prefix-symbols t1 app1.rel sender.task1
	$(GCC_PREFIX)ld -n -o image.elf -T sifive_e31.ld kernel.rel writer.task0 sender.task1

clean:
	rm -f *.o *.elf *.rel *.task* memory.ld

