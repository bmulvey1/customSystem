CC = gcc
CFLAGS = -O0 -Werror -Wall
programs: mcc

mcc: compiler.o parser.o ast.o tac.o symtab.o dict.o
	$(CC) $(CFLAGS) -o $@ $^

compiler.o: compiler.c
	$(CC) -c $(CFLAGS) compiler.c

parser.o: parser.c parser.h
	$(CC) -c $(CFLAGS) parser.c

ast.o: ast.c ast.h
	$(CC) -c $(CFLAGS) ast.c

tac.o: tac.c tac.h
	$(CC) -c $(CFLAGS) tac.c

symtab.o: symtab.c symtab.h
	$(CC) -c $(CFLAGS) symtab.c

dict.o: dict.c dict.h
	$(CC) -c $(CFLAGS) dict.c



clean:
	rm *.o
	rm ./mcc