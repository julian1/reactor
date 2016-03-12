#!/bin/bash -x

rm dispatcher.o examples/*.out

gcc -Wall -c dispatcher.c -I./ -o dispatcher.o

for i in examples/*.c; do
  gcc -Wall $i dispatcher.o -I./ -o "${i%.c}.out"
done

