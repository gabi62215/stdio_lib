build: so_stdio.o
	gcc -shared so_stdio.o -o libso_stdio.so
so_stdio.o: so_stdio.c
	gcc -Wall -c so_stdio.c -o so_stdio.o
clean:
	rm *.o libso_stdio.so