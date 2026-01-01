Black pill example
==================

This example demonstrates two tasks: blinker and sender. The sender sends 
messages to a channel every second. The blinker controls the LED.
Both tasks crash periodically due to invalid memory references, but they're
restarted by the OS and the system continues to work.

Use 

        make

to build the example. By default it uses both tasks implemented in C in
task0/task1 files. To use C++ or Rust examples in separate folder use
either (or both)

        make cpp_example

or

        make rust_example

and then call 'make' again. Then the corresponding app will be replaced
by alternative implementation in different language.

