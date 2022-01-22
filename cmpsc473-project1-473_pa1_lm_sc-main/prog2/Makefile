all: prog2_32 prog2_64

prog2_32: prog2.c
	gcc -g -m32 prog2.c -o prog2_32

prog2_64: prog2.c
	gcc -g -m64 prog2.c -o prog2_64

clean:
	rm prog2_32 prog2_64

clean_32:
	rm prog2_32

clean_64:
	rm prog2_64
