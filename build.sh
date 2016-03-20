#!/bin/bash -x

rm logger.o reactor.o examples/*.out

gcc -Wall -c logger.c -I./ -o logger.o || exit
gcc -Wall -c reactor.c -I./ -o reactor.o || exit   # change name core?
gcc -Wall -c ureactor.c -I./ -o ureactor.o || exit
gcc -Wall -c signal.c -I./ -o signal.o || exit

#for i in examples/*.c; do
for i in examples/read.c examples/signal.c; do
  gcc -Wall $i logger.o reactor.o ureactor.o signal.o -I./ -o "${i%.c}.out" || exit
done

