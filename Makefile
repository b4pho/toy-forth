.PHONY: all test

all: tforthc tforth test

test: tforthc tforth
	./tforthc examples/test.fth test.bin
	./tforth test.bin

tforthc: compiler.c
	gcc -O3 compiler.c -lm -o tforthc

tforth: vm.c
	gcc -O3 vm.c -lm -o tforth
