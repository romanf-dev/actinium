
GCC_PREFIX ?= arm-none-eabi-

all:
	cargo build --release
	mv target/thumbv7m-none-eabi/release/actinium_example app.task.o
	$(GCC_PREFIX)ld -nostdlib -r -n -T ../../../arch/armv7m/task.ld -o app.task app.task.o ../task_startup.o

clean:
	rm -f *.o
	rm -rf target

