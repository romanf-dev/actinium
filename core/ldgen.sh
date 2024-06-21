#!/bin/bash

to_hex() {
    printf "%x" "$1"
}

section_sz() {
    local sz=$(arm-none-eabi-size -A $1 | sed -n -E "s/^\\$2\s+(\w+).*/\1/p");
    echo $sz 
}

round_to_power2() {
    local counter=0
    local tmp=$1
    if [ $(( $tmp & ($tmp - 1) )) -ne 0 ]; then
        while (($tmp >> $counter)); do
            ((counter++))
        done
        f=$((1 << counter))
    else
        f=$tmp      
    fi
    if [ $f -ne 0 ]; then    
        if [ $f -lt 32 ]; then
            echo 32
        else
            echo $f
        fi
    else
        echo $f
    fi
}

alignas() {
    local low_mask=$(($2-1))
    local high_mask=$((~$low_mask))
    local tmp=$1
    if [ $(( $tmp & $low_mask )) -ne 0 ]; then
        ((tmp &= high_mask))
        ((tmp += low_mask + 1))
    fi
    echo $tmp
}

echo "Getting kernel sections info..."
ktext_sz=$(section_sz kernel.0 .text);
kdata_sz=$(section_sz kernel.0 .data);
ksram_sz=$(section_sz kernel.0 .bss);

((kdata_sz += 0))
((ktext_sz += kdata_sz))
((ksram_sz += kdata_sz))
echo Kernel code size = $ktext_sz, kernel data size = $ksram_sz.

echo "Preparing apps..."
apps_num=0
header_size=0
for f in *.o; do
    echo "Processing of $f object file as slot $apps_num..."
    arm-none-eabi-objcopy --prefix-symbols task$apps_num $f $f.pfx
    ((apps_num++))
done
((header_size = 16 * apps_num + 4))
echo "Processing of $apps_num tasks, header size = $header_size bytes."

flash_base=$1
sram_base=$2
flash_size=0
sram_size=0
echo "Device base addresses: text = $(to_hex $flash_base), data = $(to_hex $sram_base)."
echo "Linker script generation..."

echo "INPUT(kernel.0)" > input_list.ld
echo -e "MEMORY\n{\n" > memory_regions.ld
echo -e "\tKFLASH (rx) : ORIGIN = 0x$(to_hex $flash_base), LENGTH = 64K" >> memory_regions.ld
echo -e "\tKRAM (xrx) : ORIGIN = 0x$(to_hex $sram_base), LENGTH = 64K" >> memory_regions.ld
echo -e "\t.descr (READONLY) : {\n\t_ac_task_mem_map = ABSOLUTE(.);\n\tLONG($apps_num);\n" > descriptors.ld
echo -e "ENTRY(Reset_Handler)\nSECTIONS\n{\n" > sections.ld
echo -e "\t.text : {\n\t\tkernel.0(.text*)\n\t} > KFLASH\n" >> sections.ld
echo -e "\t.data : {\n\t\tkernel.0(.data*)\n\t} > KRAM AT> KFLASH\n" >> sections.ld
echo -e "\t.bss : {\n\t\tkernel.0(.bss*)\n\t\tkernel.0(COMMON)\n\t} > KRAM\n" >> sections.ld
((flash_base += ktext_sz + header_size))
((sram_base += ksram_sz))

counter=0
for f in *.o; do
    text_raw_sz=$(section_sz $f .text);
    data_raw_sz=$(section_sz $f .data);
    sram_raw_sz=$(section_sz $f .bss);
    ((text_raw_sz += data_raw_sz));
    ((sram_raw_sz += data_raw_sz));
    flash_size=$(round_to_power2 $text_raw_sz);
    sram_size=$(round_to_power2 $sram_raw_sz);
    echo "Start memory regions (unaligned): flash=$(to_hex $flash_base), sram=$(to_hex $sram_base)"
    echo "Processing $f: raw flash=$text_raw_sz, raw sram=$sram_raw_sz, rounded flash=$flash_size, sram=$sram_size"
    flash_base=$(alignas $flash_base $flash_size);
    sram_base=$(alignas $sram_base $sram_size);
    echo "INPUT($f.pfx)" >> input_list.ld
    echo -e "\tFLASH$counter (rx) : ORIGIN = 0x$(to_hex $flash_base), LENGTH = $flash_size" >> memory_regions.ld
    echo -e "\tRAM$counter (rwx) : ORIGIN = 0x$(to_hex $sram_base), LENGTH = $sram_size" >> memory_regions.ld
    echo -e "\t.text$counter : {\n\t$f.pfx(.text*)\n\t} > FLASH$counter\n" >> sections.ld
    echo -e "\t.data$counter : {\n\t$f.pfx(.data*)\n\t} > RAM$counter AT> FLASH$counter\n" >> sections.ld
    echo -e "\t.bss$counter : {\n\t$f.pfx(.bss*)\n\t} > RAM$counter\n" >> sections.ld
    echo -e "\tLONG(0x$(to_hex $flash_base));\n\tLONG(0x$(to_hex $flash_size));\n" >> descriptors.ld
    echo -e "\tLONG(0x$(to_hex $sram_base));\n\tLONG(0x$(to_hex $sram_size));\n" >> descriptors.ld
    echo "Task$counter from file $f is placed at flash=0x$(to_hex $flash_base) sram=0x$(to_hex $sram_base)."
    ((flash_base += flash_size))
    ((sram_base += sram_size))
    ((counter++))
done
echo -e "}\n" >> memory_regions.ld
echo -e "\t} > KFLASH\n}\n" >> descriptors.ld

echo "Linking..."
cat input_list.ld memory_regions.ld sections.ld descriptors.ld > ldscript.ld
arm-none-eabi-ld -n -o image.elf -T ldscript.ld
rm input_list.ld memory_regions.ld sections.ld descriptors.ld *.pfx

