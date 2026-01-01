GCC_PREFIX ?= riscv32-unknown-elf-
AC_PATH ?= ../..
PORT_PATH = $(AC_PATH)/arch/rv32
INCLUDE_DIRS = -I $(PORT_PATH) -I $(AC_PATH) -I $(AC_PATH)/magnesium
PICOTOOL_PATH ?= echo PICOTOOL_PATH is not defined. To get uf2 use picotool

.PHONY: all clean kernel
.DEFAULT_GOAL := all

%.o : %.c
	$(GCC_PREFIX)gcc -ffreestanding -march=rv32ima_zicsr_zbb -Wall -Wextra -O2 -DMG_CPU_MAX=2 $(INCLUDE_DIRS) -I . -c -o $@ $<

%.o : %.s
	$(GCC_PREFIX)gcc -march=rv32ima_zicsr -c -o $@ $<

%.o : $(PORT_PATH)/%.s
	$(GCC_PREFIX)gcc -march=rv32izicsr -c -o $@ $<

app%.rel : task%.c task_startup.o
	$(GCC_PREFIX)gcc -ffreestanding -Wall -O2 -march=rv32i -I $(AC_PATH)/usr/c -I . -c -o $@.o $<
	$(GCC_PREFIX)ld -nostdlib -r -n -T $(PORT_PATH)/task.ld -o $@ $@.o task_startup.o

all : clean kernel app0.rel app1.rel
	$(AC_PATH)/ldgen.sh $(GCC_PREFIX)size kernel.rel app0.rel app1.rel > memory.ld
	$(GCC_PREFIX)objcopy --prefix-symbols t0 app0.rel blinker.task0
	$(GCC_PREFIX)objcopy --prefix-symbols t1 app1.rel sender.task1
	$(GCC_PREFIX)ld --no-relax-gp -n -o rp2350.elf -T rp2350.ld kernel.rel blinker.task0 sender.task1
	$(PICOTOOL_PATH)/picotool uf2 convert --verbose rp2350.elf rp2350.uf2 --family 0xe48bff5a

kernel : main.o core1.o startup.o traps.o
	$(GCC_PREFIX)ld -nostdlib -r -n -T kernel.ld -o kernel.rel main.o core1.o startup.o traps.o

clean :
	rm -f *.o *.elf *.rel *.task* *.uf2 memory.ld

