
GCC_PREFIX ?= arm-none-eabi-
AC_PATH ?= ../..
PORT_PATH = $(AC_PATH)/arch/armv7m
INCLUDE_DIRS = -I $(PORT_PATH) -I $(AC_PATH) -I $(AC_PATH)/magnesium -I $(AC_PATH)/magnesium/nvic -I CMSIS

.PHONY: all clean tasks kernel

%.o : %.c
	$(GCC_PREFIX)gcc -ffreestanding -mcpu=cortex-m4 -Wall -DSTM32F401xC $(INCLUDE_DIRS) -O2 -mthumb -I . -c -o $@ $<

%.o : $(PORT_PATH)/%.s
	$(GCC_PREFIX)gcc -mcpu=cortex-m4 -mthumb -c -o $@ $<

app0.task : task_startup.o
	$(MAKE) --directory=task0_usb_cdc
	$(GCC_PREFIX)ld -r -n -T $(PORT_PATH)/task.ld -o $@ task0_usb_cdc/app.task task_startup.o

app%.task : task%.c task_startup.o
	$(GCC_PREFIX)gcc -ffreestanding -mcpu=cortex-m4 -Wall -I $(AC_PATH)/usr/c -I CMSIS -DSTM32F401xC -O2 -mthumb -I . -c -o $@.o $<
	$(GCC_PREFIX)ld -nostdlib -r -n -T $(PORT_PATH)/task.ld -o $@ $@.o task_startup.o

%.o : %.s
	$(GCC_PREFIX)gcc -mcpu=cortex-m4 -mthumb -c -o $@ $<

all : clean app0.task app1.task app2.task kernel
	$(AC_PATH)/ldgen.sh 0x08000000 0x20000000 $(GCC_PREFIX)
	$(GCC_PREFIX)ld -n -o image.elf -T ldscript.ld

kernel : main.o startup_stm32f401xc.o traps.o
	$(GCC_PREFIX)ld -nostdlib -r -n -T stm32f401.ld -o kernel.0 main.o startup_stm32f401xc.o traps.o

clean :
	rm -f *.o *.task *.elf *.0 *.pfx ldscript.ld
	$(MAKE) --directory=task0_usb_cdc clean

