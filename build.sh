#!/bin/bash -x

rm logger.o reactor.o examples/*.out

for i in src/*.c; do
  gcc -Wall $i -c -I./include -o "${i%.c}.o" || exit
done
#exit


#for i in examples/*.c; do
for i in examples/read.c examples/signal.c; do
  gcc -Wall $i src/*.o  -I./include -o "${i%.c}.out" || exit
done

