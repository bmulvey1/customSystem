CC = gcc
CFLAGS = -g -Werror -Wall
programs: mcc

mcc: compiler.o parser.o ast.o tac.o
	$(CC) $(CFLAGS) -o $@ $^

compiler.o: compiler.c
	$(CC) -c $(CFLAGS) compiler.c

parser.o: parser.c parser.h
	$(CC) -c $(CFLAGS) parser.c

ast.o: ast.c ast.h
	$(CC) -c $(CFLAGS) ast.c

tac.o: tac.c tac.h
	$(CC) -c $(CFLAGS) tac.c



clean:
	rm *.o
	rm ./emu