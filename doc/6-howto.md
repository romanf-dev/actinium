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

After code is compiled into relocatables, rename kernel relocatable into kernel.0
(numeric 0, not O).

Run ldgen.sh script with three parameters as base addresses of flash and SRAM 
plus toolchain prefix.
Example for STM32F401:

        ldgen.sh 0x08000000 0x20000000 arm-none-eabi-

Ldgen script treats object files (*.task) in the folder as applications sorted 
alphabetically. That is the first file in alphabatical order will become actor
with task_id = 0, second - with task_id = 1, etc. It produces a set of 
temporary files and ldscript.ld.

Finally, run linker with the ldscript generated at the previous step. Note that
object files are already mentioned in the script as INPUT, so, don't specify
anything except the script in the linker command line.

If linker succeeds it creates image containing the kernel and apps that is 
ready for flashing.



How to write applications
-------------------------

TODO

