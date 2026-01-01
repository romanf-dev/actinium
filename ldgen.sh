#!/bin/bash

section_sz() {
    local sz=$($1 -A $2 | sed -n -E "s/^\\$3\s+(\w+).*/\1/p");
    echo $sz 
}

round_to_power2() {
    local tmp=$1
    ((tmp |= 31))
    ((tmp |= $tmp >> 1))
    ((tmp |= $tmp >> 2))
    ((tmp |= $tmp >> 4))
    ((tmp |= $tmp >> 8))
    ((tmp |= $tmp >> 16))
    ((tmp += 1))
    echo $tmp
}

sz_cmd=$1
ktext_sz=$(section_sz $sz_cmd $2 .text);
kdata_sz=$(section_sz $sz_cmd $2 .data);
ksram_sz=$(section_sz $sz_cmd $2 .bss);

((kdata_sz += 0))
((ktext_sz += kdata_sz))
((ksram_sz += kdata_sz))

echo -e "KERN_RO_SZ = $ktext_sz;"
echo -e "KERN_RW_SZ = $ksram_sz;"

counter=0
for ((i=3; i<=$#; i++)); do
    f=${!i};
    text_raw_sz=$(section_sz $sz_cmd $f .text);
    data_raw_sz=$(section_sz $sz_cmd $f .data);
    sram_raw_sz=$(section_sz $sz_cmd $f .bss);
    ((text_raw_sz += data_raw_sz));
    ((sram_raw_sz += data_raw_sz));
    flash_size=$(round_to_power2 $text_raw_sz);
    sram_size=$(round_to_power2 $sram_raw_sz);
    echo -e "TASK${counter}_RO_SZ = $flash_size;"
    echo -e "TASK${counter}_RW_SZ = $sram_size;"
    ((counter++))
done

echo -e "TASK_NUM = $counter;"

