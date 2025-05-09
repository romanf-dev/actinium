
GCC_PREFIX ?= arm-none-eabi-
AC_PATH ?= ../..
PORT_PATH = $(AC_PATH)/arch/armv8m
INCLUDE_DIRS = -I $(PORT_PATH) -I $(AC_PATH) -I $(AC_PATH)/magnesium -I $(AC_PATH)/magnesium/nvic
PICOTOOL_PATH ?= echo PICOTOOL_PATH is not defined. To get uf2 use picotool

.PHONY: all clean kernel

%.o : %.c
	$(GCC_PREFIX)gcc -ffreestanding -mcpu=cortex-m33 -mcmse -mthumb -O2 -DMG_CPU_MAX=2 -DMG_NVIC_PRIO_BITS=4 \
	-Wall $(INCLUDE_DIRS) -Wl,--build-id=none -I . -I CMSIS/Device/RP2350/Include -I CMSIS/Core/Include \
	-c -o $@ $<

%.o : %.s
	$(GCC_PREFIX)gcc -mcpu=cortex-m33 -mthumb -c -o $@ $<

%.o : $(PORT_PATH)/%.s
	$(GCC_PREFIX)gcc -mcpu=cortex-m33 -mthumb -c -o $@ $<

app%.task : task%.c 
	$(GCC_PREFIX)gcc -ffreestanding -Wall -O2  -mcpu=cortex-m33 -mthumb -I $(AC_PATH)/usr/c -I CMSIS/Device/RP2350/Include -I CMSIS/Core/Include -I . -c -o $@.o $<
	$(GCC_PREFIX)ld -nostdlib -r -n -T $(PORT_PATH)/task.ld -o $@ $@.o task_startup.o

all : clean kernel task_startup.o app0.task app1.task
	$(AC_PATH)/ldgen.sh 0x10000000 0x20000000 $(GCC_PREFIX)
	$(GCC_PREFIX)ld -n -o image.elf -T ldscript.ld
	$(PICOTOOL_PATH) uf2 convert --verbose image.elf demo.uf2 --family rp2350-arm-s

kernel : main.o core1.o startup.o traps.o
	$(GCC_PREFIX)ld -nostdlib -r -n -T rp2350.ld -o kernel.0 main.o core1.o startup.o traps.o

clean :
	rm -f *.o *.elf *.0 *.task *.pfx *.uf2 ldscript.ld

