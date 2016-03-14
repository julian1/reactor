#!/bin/bash -x

rm reactor.o examples/*.out

gcc -Wall -c reactor.c -I./ -o reactor.o

for i in examples/*.c; do
  gcc -Wall $i reactor.o -I./ -o "${i%.c}.out"
done

