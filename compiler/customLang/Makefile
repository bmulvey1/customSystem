CC = gcc
CFLAGS = -Werror -Wall
programs: mcc

mcc: compiler.o
	$(CC) $(CFLAGS) -o $@ $^

compiler.o: compiler.c
	$(CC) -c $(CFLAGS) compiler.c

clean:
	rm *.o
	rm ./emu