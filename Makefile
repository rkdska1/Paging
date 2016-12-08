fact1: file1.c file2.c
	gcc	-o fact1 file1.c file2.c 

fact2: file1.o file2.o
	gcc -o fact2 file1.o file2.o

file1.o: file1.c
	gcc -c file1.c

file2.o: file2.c
	gcc -c file2.c

%.o: %.c
	gcc -c -o $@ $<

fact3: file1.o file2.o
	gcc -o $@ $^
