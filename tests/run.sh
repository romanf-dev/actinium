#!/bin/bash

for f in *.c; do
    gcc -pedantic -std=c11 -Wall -O2 -g -Wall -I ../core -I ../core/arch/test -I ../magnesium -o $f.tst $f
done

for f in *.tst; do
    ./$f
    if [ $? -eq 0 ]; then
        echo $f: OK
    fi
done

rm -f *.tst

