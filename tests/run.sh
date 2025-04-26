#!/bin/bash

for f in *.c; do
    gcc -pedantic -std=c11 -Wall -O2 -g -I .. -I ../arch/test -I ../magnesium -o $f.tst $f
done

for f in *.tst; do
    ./$f
    if [ $? -eq 0 ]; then
        echo $f: OK
    fi
done

rm -f *.tst

