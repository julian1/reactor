#!/bin/bash -x

rm logger.o reactor.o examples/*.out

gcc -Wall -c logger.c -I./ -o logger.o
gcc -Wall -c reactor.c -I./ -o reactor.o

for i in examples/*.c; do
  gcc -Wall $i reactor.o -I./ -o "${i%.c}.out"
done

