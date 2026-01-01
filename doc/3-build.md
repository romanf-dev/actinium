Build process
=============

Building is a little bit complex since we have to pack many semi-independent
tasks into single ELF with proper alignment. 

First, the core part containing MCU initialization is compiled using its
linker script into relocatable kernel.rel file. This file is expected to have 
vector table and have to be placed first.

Second, user tasks are compiled into regular relocatable object files using
framework-provided linker script. It has no deal with addresses, just
rearrange sections.

All relocatable files should have sections text/data/bss (possibly zero-sized),
other allocatable sections are ignored.
Depending on actor implementation language it is sometime needed to rename/group
sections before using task's linker script as it expects some well-defined
structure of the relocatable. This step is called prelinking.

Third, since tasks may have symbol name conflicts they are prefixed with unqiue
prefix with objcopy binutil. ldgen.sh script extracts section sizes to be used
in final linking stage (with user-provided script) that links the kernel and
all the tasks together.

Typical build process is shown below:

- main.c, initialization code and any other privileged functions are compiled
as usual into ELF relocatable file. Examples use kernel.rel file name.

- unprivileged tasks are also compiled separately and linked into relocatable
executables (with the same .rel extension). If implementation language is not
C they possibly prelinked with specific linker script.

- ldgen script extracts tasks sizes, rounds them to power of 2 and generate
file with the sizes to be included into the final linker script.

- each task relocatable is prefixed by objcopy to make each symbol unique and
to resolve possible conflicts. These files have unique extensions like .task0
.task1 etc. 

- all the prefixed tasks and the kernel are linked together using final linker
script describing target memory where tasks are properly aligned.

