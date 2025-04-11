#
# Simple makefile for compiling all .c files in the current folder.
#

SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,%.o,$(SRCS))

GCC_PREFIX ?= riscv32-unknown-elf-
PICOTOOL_PATH ?= echo "\e[33mPICOTOOL_PATH is not defined.\e[0m" To get uf2 use picotool
MG_PATH ?= ..

.PHONY: all clean fwk kernel

%.o: %.c
	$(GCC_PREFIX)gcc -ffreestanding -march=rv32ima_zicsr_zbb -Wall -Wextra -O2 \
	-DMG_CPU_MAX=2 -I ../core -I ../core/arch/rv32 -I ../magnesium -I . -c -o $@ $<

%.o: %.s
	$(GCC_PREFIX)gcc -march=rv32ima_zicsr -c -I . -o $@ $<

task%.o: task%.c
	$(GCC_PREFIX)gcc -ffreestanding -Wall -O2 -march=rv32i -I ../core/c_task -I . -c -o $@ $<

app%.o: task%.o
	$(GCC_PREFIX)ld -nostdlib -r -n -T ../core/arch/rv32/task.ld -o $@ $< ../core/arch/rv32/task_startup.o

all: clean kernel app0.o app1.o
	rm main.o core1.o startup.o task0.o task1.o
	../core/ldgen.sh 0x10000000 0x20000000 $(GCC_PREFIX)
	$(GCC_PREFIX)ld --no-relax-gp -n -o image.elf -T ldscript.ld
	$(PICOTOOL_PATH) uf2 convert --verbose image.elf demo.uf2 --family 0xe48bff5a

fwk: 
	$(MAKE) --directory=../core/arch/rv32

kernel: main.o core1.o startup.o fwk
	$(GCC_PREFIX)ld -nostdlib -r -n -T rp2350.ld -o kernel.0 main.o core1.o startup.o ../core/arch/rv32/actinium.o

clean:
	rm -f *.o *.elf *.0 *.pfx *.uf2 ldscript.ld
	$(MAKE) --directory=../core/arch/rv32 clean

