Black pill example
==================

The demo contains two tasks: sender and controller. The 'sender' sends
requests to toggle LED to 'controller' who has access to the corresponding 
peripheral. Both tasks crash periodically after few activations but they're
restarted by exceptions and the system continues to work.
This demonstrates 'let it crash' principle: even when no task is reliable 
the whole system works as designed.
To build the demo run

        make

in the demo folder (provided that arm-none-eabi- toolchain is available via 
PATH). This yields image.elf file containing all the tasks and the kernel
ready for flashing.

Original tasks (task0.c and task1.c) are simple and written in C. However,
since actors are inherently 'async' writing them in C requres some efforts
because of no async/await functionality in the language. 

There are two demo modifications: 'sender' in C++20 and 'controller' in Rust.

Modified controller interprets 'LED on' and 'LED off' messages as 'blink once' 
and 'blink  twice' so it is visually distinguishable from C demo. 
To build the demo modification run:

        make rust_example

and/or: 

        make cpp_example

Then run:

        make

To build the executable image with modified tasks.

