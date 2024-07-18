Memory regions and MPU
======================

Currently, 5 regions are used for each unprivileged actor.
- Code (flash)
- Data (SRAM, also includes .bss)
- Stack
- Currently owned message
- ‘User’ region for peripheral access (optional)


Because of hardware restrictions of the MPU, messages should be:
- at least 32 bytes size
- aligned to its size
- sized to power of 2

A task may have access to a single message at any moment.
After flashing the MCU memory looks like the following:



               Memory layout                 MPU regions when Task 0 runs

        +--------------------------+         +--------------------------+
        |        ISR stack         |         |                          |
        +--------------------------+         +--------------------------+
        |           ...            |         |                          |
        +--------------------------+         +--------------------------+
        |    Task n data & bss     |         |                          |
        +--------------------------+         +--------------------------+
        |    Task 0 data & bss     |         |    Task 0 data & bss     |
        +--------------------------+         +--------------------------+
        |Stacks for each prio level|         | Stack for T0 prio |      |
        +--------------------------+         +--------------------------+
        |  Aligned Message pools   |         | Owned msg |  no access   |
        +--------------------------+         +--------------------------+
        | Core data, runqueues, etc|         |                          |
        +--------------------------+ <-SRAM  +--------------------------+
        |           ...            |         |                          |
        +--------------------------+         +--------------------------+
        |   Task n code & rodata   |         |                          |
        +--------------------------+         +--------------------------+
        |   Task 0 code & rodata   |         |   Task 0 code & rodata   |
        +--------------------------+         +--------------------------+
        |  Core code, ISRs, init   |         |                          |
        +--------------------------+         +--------------------------+
        |       ISR vectors        |         |                          |
        +--------------------------+ <-FLASH +--------------------------+


