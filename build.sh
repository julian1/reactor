#!/bin/bash -x

gcc -Wall -c dispatcher.c -I./ -o dispatcher.o

rm examples/*.out

for i in examples/*.c; do
  gcc -Wall $i dispatcher.o -I./ -o "${i%.c}.out"
done

