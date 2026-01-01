GCC_PREFIX ?= arm-none-eabi-
AC_PATH ?= ../..
PORT_PATH = $(AC_PATH)/arch/armv7m
INCLUDE_DIRS = -I $(PORT_PATH) -I $(AC_PATH) -I $(AC_PATH)/magnesium -I $(AC_PATH)/magnesium/nvic -I CMSIS

.DEFAULT_GOAL := all
.PHONY: all clean flash kernel

%.o : %.c
	$(GCC_PREFIX)gcc -ffreestanding -mcpu=cortex-m4 -Wall -DSTM32F401xC $(INCLUDE_DIRS) -O2 -mthumb -I . -c -o $@ $<

%.o : $(PORT_PATH)/%.s
	$(GCC_PREFIX)gcc -mcpu=cortex-m4 -mthumb -c -o $@ $<

%.o : %.s
	$(GCC_PREFIX)gcc -mcpu=cortex-m4 -mthumb -c -o $@ $<

kernel : main.o startup_stm32f401xc.o traps.o
	$(GCC_PREFIX)ld -nostdlib -r -n -T kernel.ld -o kernel.rel main.o startup_stm32f401xc.o traps.o

usbserv.rel : task_startup.o
	$(MAKE) --directory=task0_usb_cdc
	$(GCC_PREFIX)ld -r -n -T $(PORT_PATH)/task.ld -o $@ task0_usb_cdc/usb.rel task_startup.o

app%.rel : task%.c task_startup.o
	$(GCC_PREFIX)gcc -ffreestanding -mcpu=cortex-m4 -Wall -I $(AC_PATH)/usr/c -I CMSIS -DSTM32F401xC -O2 -mthumb -I . -c -o $@.o $<
	$(GCC_PREFIX)ld -nostdlib -r -n -T $(PORT_PATH)/task.ld -o $@ $@.o task_startup.o

all : clean usbserv.rel app1.rel app2.rel kernel
	$(AC_PATH)/ldgen.sh $(GCC_PREFIX)size kernel.rel usbserv.rel app1.rel app2.rel > memory.ld
	$(GCC_PREFIX)objcopy --prefix-symbols t0 usbserv.rel usbserv.task0
	$(GCC_PREFIX)objcopy --prefix-symbols t1 app1.rel app.task1
	$(GCC_PREFIX)objcopy --prefix-symbols t2 app2.rel gpioserv.task2
	$(GCC_PREFIX)ld -n -o image.elf -T stm32f401.ld kernel.rel usbserv.task0 app.task1 gpioserv.task2

clean :
	rm -f *.o *.task* *.elf *.rel memory.ld
	$(MAKE) --directory=task0_usb_cdc clean

flash : all
	openocd -f interface/stlink-v2.cfg -f target/stm32f4x.cfg -c "program image.elf verify reset exit"

