How to use
==========

- Set include dirs and include actinium.h into the application.
- Add global variables g_mg_context and g_ac_context to some file in the project.
- Install ac_port_intr_entry as interrupt handler for vectors dedicated to actors.
- Install ac_port_trap_entry and ac_port_svc_entry as handlers for exceptions and syscall.
- Initialize interrupt controller registers and priorities.
- Initialize context, channels and actors in main().
- Put call to 'tick' in interrupt handler of the tick source.
- Implement the validation function that maps ids to channel objects.
- Call ac_kernel_start at end of main().


Run ldgen.sh script with the following parameters:

        ldgen.sh arm-none-eabi-size kernel.o task1.o task2.o ...

This yields RO/RW sizes for kernel and each task rounded to power of 2. Then use final
linker script for target board combining all tasks and the kernel into a single image.


How to write applications
-------------------------

TODO

