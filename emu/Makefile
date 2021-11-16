CC = g++
CFLAGS = -Wall -Werror
programs: emu

emu: emu.o
	$(CC) $(CFLAGS) -o $@ $^

emu.o: emu.cpp
	$(CC) -c $(CFLAGS) emu.cpp

clean:
	rm *.o
	rm ./emu