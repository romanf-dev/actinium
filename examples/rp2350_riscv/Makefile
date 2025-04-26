
GCC_PREFIX ?= riscv32-unknown-elf-
AC_PATH ?= ../..
PORT_PATH = $(AC_PATH)/arch/rv32
INCLUDE_DIRS = -I $(PORT_PATH) -I $(AC_PATH) -I $(AC_PATH)/magnesium
PICOTOOL_PATH ?= echo PICOTOOL_PATH is not defined. To get uf2 use picotool

.PHONY: all clean kernel

%.o : %.c
	$(GCC_PREFIX)gcc -ffreestanding -march=rv32ima_zicsr_zbb -Wall -Wextra -O2 -DMG_CPU_MAX=2 $(INCLUDE_DIRS) -I . -c -o $@ $<

%.o : %.s
	$(GCC_PREFIX)gcc -march=rv32ima_zicsr -c -o $@ $<

%.o : $(PORT_PATH)/%.s
	$(GCC_PREFIX)gcc -march=rv32izicsr -c -o $@ $<

app%.task : task%.c 
	$(GCC_PREFIX)gcc -ffreestanding -Wall -O2 -march=rv32i -I $(AC_PATH)/usr/c -I . -c -o $@.o $<
	$(GCC_PREFIX)ld -nostdlib -r -n -T $(PORT_PATH)/task.ld -o $@ $@.o task_startup.o

all : clean kernel task_startup.o app0.task app1.task
	$(AC_PATH)/ldgen.sh 0x10000000 0x20000000 $(GCC_PREFIX)
	$(GCC_PREFIX)ld --no-relax-gp -n -o image.elf -T ldscript.ld
	$(PICOTOOL_PATH) uf2 convert --verbose image.elf demo.uf2 --family 0xe48bff5a

kernel : main.o core1.o startup.o traps.o
	$(GCC_PREFIX)ld -nostdlib -r -n -T rp2350.ld -o kernel.0 main.o core1.o startup.o traps.o

clean :
	rm -f *.o *.elf *.0 *.task *.pfx *.uf2 ldscript.ld

