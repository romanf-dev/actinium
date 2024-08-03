Build process
=============

Building is a little bit complex since we have to pack many semi-independent
tasks into single ELF with proper alignment. 

First, the core part containing MCU initialization is compiled using its
linker script into relocatable kernel.0 file. Note that extension is 
numeric 0, not o. This file is expected to have vector table and have to be
placed first.

Second, user tasks are compiled into regular relocatable object files using
framework-provided linker script. It has no deal with addresses, just
rearrange sections.
All relocatable files should have sections text/data/bss (possibly zero-sized),
other allocatable sections are ignored.

Third, ldgen.sh script extracts section sizes for *.o and kernel.0 files and
generates linker script packing the files together with proper alignment.
Also it provides section sizes in a special symbol so kernel may
further use this info when spawning tasks.


               +-----------+    text/data/bss sizes   +------------------+
               |  task.ld  |        +---------------->|      ldgen.sh    |
               +-----------+        |            ^    +------------------+
                     |              |            |           |
         +--------+  V  +----------------------+ |           |  section align
         | task.c |---->| relocatable  task.o  |-+-+         V  & relocation
         +--------+     +----------------------+ | |  +------------------+
                                                 | |  | generated script |
               +-----------+              +------+ |  +------------------+        
               | kernel.ld |              |        |         |  final linking
               +-----------+              |        |         V
                     |                    |        |  +------------------+
         +--------+  V  +----------------------+   V  |    executable    |
         | main.c |---->| relocatable kernel.0 |----->|       image      |
         +--------+     +----------------------+      +------------------+


