SiFive E31 QEMU example
=======================

This example demonstrates two tasks: writer and sender. The sender sends 
messages to a channel every second. The writer outputs the message content
into the serial port.
Both tasks crash periodically due to invalid memory references, but they're
restarted by the OS and the system continues to work.

The command line for running QEMU:

        qemu-system-riscv32 -machine sifive_e -cpu sifive-e31 -nographic -kernel image.elf

It should work like this: (use Ctrl+A then X to quit QEMU)

user@pc:~/qemu-riscv-8.2.2-1/bin$ qemu-system-riscv32 -machine sifive_e -cpu sifive-e31 -nographic -kernel image.elf
hello!
hello!
hello!
kernel: task crash, restart
hello!
hello!
hello!
kernel: task crash, restart
hello!
hello!
hello!
kernel: task crash, restart
hello!
hello!
QEMU: Terminated

