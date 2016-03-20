#!/bin/bash -x

rm src/*.o examples/*.out

for i in src/*.c; do
  gcc -Wall $i -c -I./include -o "${i%.c}.o" || exit
done

#for i in examples/*.c; do
for i in examples/read.c examples/signal.c examples/timeout.c; do
  gcc -Wall $i src/*.o  -I./include -o "${i%.c}.out" || exit
done

