cmake_minimum_required(VERSION 3.20)
project(kernel LANGUAGES C ASM)

# Set the toolchain.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv)

# Specify the cross compiler.
set(CMAKE_C_COMPILER riscv32-unknown-elf-gcc)
set(CMAKE_ASM_COMPILER riscv32-unknown-elf-gcc)
set(CMAKE_OBJCOPY riscv32-unknown-elf-objcopy)
set(CMAKE_SIZE riscv32-unknown-elf-size)
set(LINKER riscv32-unknown-elf-ld)

# Global compiler flags for bare metal ARM.
set(CMAKE_C_FLAGS "-ffreestanding -Wall -O2 -nostartfiles -nodefaultlibs")
set(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS}")
set(CMAKE_VERBOSE_MAKEFILE OFF)

set(PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(AC_DIR ${PROJECT_DIR}/../../)
set(PORT_DIR ${AC_DIR}/arch/rv32)

set(KERNEL_SOURCES main.c core1.c startup.s ${PORT_DIR}/traps.s)

# Privileged code is compiled into separate relocatable binary kernel.rel.
add_executable(kernel.rel ${KERNEL_SOURCES})
target_include_directories(kernel.rel PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${AC_DIR}
    ${AC_DIR}/magnesium/
    ${PORT_DIR}
)
target_compile_definitions(kernel.rel PRIVATE MG_CPU_MAX=2)
target_compile_options(kernel.rel PRIVATE -march=rv32ima_zicsr_zbb)
target_link_options(kernel.rel PRIVATE -nostdlib -Wl,-r -Wl,-n -T ${PROJECT_DIR}/kernel.ld)

# Task startup contains initialization code and has to be placed first inside the task binary.
add_library(task_startup OBJECT ${PORT_DIR}/task_startup.s)
target_compile_options(task_startup PRIVATE -march=rv32izicsr)

# Blinking task.
add_executable(blinker.rel task0.c)
add_dependencies(blinker.rel task_startup)
target_compile_options(blinker.rel PRIVATE -march=rv32i)
target_include_directories(blinker.rel PRIVATE 
    ${AC_DIR}/usr/c
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(blinker.rel $<TARGET_OBJECTS:task_startup>)
target_link_options(blinker.rel PRIVATE -nostdlib -Wl,-r -Wl,-n -T ${PORT_DIR}/task.ld)
add_custom_command(TARGET blinker.rel POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} --prefix-symbols t0 blinker.rel blinker.task0
    COMMENT "Generating blinker task..."
)
set_property(TARGET blinker.rel APPEND PROPERTY ADDITIONAL_CLEAN_FILES blinker.task0)

# Sender task.
add_executable(sender.rel task1.c)
add_dependencies(sender.rel task_startup)
target_compile_options(sender.rel PRIVATE -march=rv32i)
target_link_libraries(sender.rel $<TARGET_OBJECTS:task_startup>)
target_link_options(sender.rel PRIVATE -nostdlib -Wl,-r -Wl,-n -T ${PORT_DIR}/task.ld)
target_include_directories(sender.rel PRIVATE 
    ${AC_DIR}/usr/c
    ${CMAKE_SOURCE_DIR}
)
add_custom_command(TARGET sender.rel POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} --prefix-symbols t1 sender.rel sender.task1
    COMMENT "Generating sender task..."
)
set_property(TARGET sender.rel APPEND PROPERTY ADDITIONAL_CLEAN_FILES sender.task1)

add_custom_target(image.elf ALL DEPENDS kernel.rel blinker.rel sender.rel  
    COMMAND ${AC_DIR}/ldgen.sh ${CMAKE_SIZE} kernel.rel blinker.rel sender.rel > memory.ld
	COMMAND ${LINKER} --no-relax-gp -n -o image.elf -T ${PROJECT_DIR}/rp2350.ld kernel.rel blinker.task0 sender.task1
    COMMENT "Linking the kernel and tasks together..."
)
set_property(TARGET image.elf APPEND PROPERTY ADDITIONAL_CLEAN_FILES image.elf memory.ld)

# Find picotool.
find_program(PICOTOOL_EXECUTABLE
    NAMES picotool
    HINTS
        $ENV{PICO_SDK_PATH}/../picotool/build
        $ENV{PICOTOOL_PATH}
    DOC "Path to picotool executable"
    OPTIONAL
)

# Function to create UF2 target from any ELF target.
function(add_uf2_target TARGET_NAME)
    set(ELF_FILE ${TARGET_NAME})
    set(UF2_FILE ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.uf2)
    
    add_custom_command(
        OUTPUT ${UF2_FILE}
        COMMAND ${PICOTOOL_EXECUTABLE} uf2 convert --verbose ${ELF_FILE} ${UF2_FILE} --family 0xe48bff5a
        DEPENDS ${TARGET_NAME}
        COMMENT "Generating ${TARGET_NAME}.uf2"
        VERBATIM
    )
    
    add_custom_target(${TARGET_NAME}_uf2 ALL DEPENDS ${UF2_FILE})
endfunction()

# Usage: Create UF2 from image.elf.
if (PICOTOOL_EXECUTABLE)
    message(STATUS "Found picotool: ${PICOTOOL_EXECUTABLE}")
    add_uf2_target(image.elf)
else()
    message(WARNING "picotool not found. UF2 target will not be available.")
endif()

