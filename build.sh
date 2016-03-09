#!/bin/bash -x 

gcc -Wall main.c dispatcher.c -I./

gcc -Wall test2.c dispatcher.c -I./ -o test2.out
gcc -Wall test3.c dispatcher.c -I./ -o test3.out

gcc -Wall test4.c dispatcher.c -I./ -o test4.out
gcc -Wall test5.c dispatcher.c -I./ -o test5.out

