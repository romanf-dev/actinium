RP2350 ARM example
==================

This example demonstrates two tasks: blinker and sender. The sender sends 
messages to a channel every 100ms. The blinker controls the LED.
The blinker crashes periodically due to invalid memory references, but it is
restarted by the OS and the system continues to work.

Two build options are available: classic make and CMake.

To use make just call 'make' in the project folder. It is assumed that 
arm-none-eabi- GNU toolchain is available via PATH. Also, if optional 
PICOTOOL_PATH is set then build produces UF2 file too, otherwise only
ELF will be built.

To use CMake use the following commands in the project folder:

        mkdir build
        cmake ..
        make

It is lso assumed that GNU toolchain is available via PATH.

