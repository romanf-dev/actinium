GCC_PREFIX ?= arm-none-eabi-
AC_PATH ?= ../..
PORT_PATH = $(AC_PATH)/arch/armv8m
INCLUDE_DIRS = -I $(PORT_PATH) -I $(AC_PATH) -I $(AC_PATH)/magnesium -I $(AC_PATH)/magnesium/nvic
CMSIS_DIRS = -I. -I CMSIS/Device/RP2350/Include -I CMSIS/Core/Include
PICOTOOL_PATH ?= echo PICOTOOL_PATH is not defined. To get uf2 use
CC_FLAGS = -ffreestanding -mcpu=cortex-m33 -Wall -O2

.DEFAULT_GOAL := all
.PHONY: all clean kernel

%.o : %.c
	$(GCC_PREFIX)gcc $(CC_FLAGS) -DMG_CPU_MAX=2 -DMG_NVIC_PRIO_BITS=4 -mcmse $(INCLUDE_DIRS) $(CMSIS_DIRS) -c -o $@ $<

%.o : %.s
	$(GCC_PREFIX)gcc -mcpu=cortex-m33 -mthumb -c -o $@ $<

%.o : $(PORT_PATH)/%.s
	$(GCC_PREFIX)gcc -mcpu=cortex-m33 -mthumb -c -o $@ $<

app%.rel : task%.c task_startup.o
	$(GCC_PREFIX)gcc $(CC_FLAGS) -I $(AC_PATH)/usr/c $(CMSIS_DIRS) -c -o $@.o $<
	$(GCC_PREFIX)ld -nostdlib -r -n -T $(PORT_PATH)/task.ld -o $@ $@.o task_startup.o

kernel : main.o core1.o startup.o traps.o
	$(GCC_PREFIX)ld -nostdlib -r -n -T kernel.ld -o kernel.rel main.o core1.o startup.o traps.o

all : clean kernel app0.rel app1.rel
	$(AC_PATH)/ldgen.sh $(GCC_PREFIX)size kernel.rel app0.rel app1.rel > memory.ld
	$(GCC_PREFIX)objcopy --prefix-symbols t0 app0.rel app.task0
	$(GCC_PREFIX)objcopy --prefix-symbols t1 app1.rel app.task1
	$(GCC_PREFIX)ld -n -o rp2350.elf -T rp2350.ld kernel.rel app.task0 app.task1
	$(PICOTOOL_PATH)/picotool uf2 convert --verbose rp2350.elf rp2350.uf2 --family rp2350-arm-s

clean :
	rm -f *.o *.elf *.rel *.task* *.uf2 memory.ld

