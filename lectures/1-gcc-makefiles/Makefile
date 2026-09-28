
program: objs #try multiple precondition
	gcc main.o aux.o -o program

preprocess: main.c aux.c
	gcc -E main.c -o main.i
	gcc -E aux.c -o aux.i

assembly: preprocess
	gcc -S *.c

assembler: assembly 
	gcc -c *.s

objs: assembler
	gcc -c *.c 

clean:
	rm -f *.s *.i *.o program