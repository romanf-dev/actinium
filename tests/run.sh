#!/bin/bash

error() {
    rm -f *.test
    exit 1
}

for f in *.c; do
    gcc -pedantic -std=c11 -Wall -O2 -g -I .. -I ../arch/test -I ../magnesium -o $f.test $f
    if [ $? -ne 0 ]; then error 
    fi
    ./$f.test
    if [ $? -ne 0 ]; then error 
    fi
    echo $f: OK
done

rm -f *.test

