CC = g++ 
CFLAGS = -Werror -Wall -O2 -g --std=c++17
programs: emu

emu: emu.o memory.o
	$(CC) $(CFLAGS) -o $@ $^

emu.o: emu.cpp
	$(CC) -c $(CFLAGS) emu.cpp 

memory.o: memory.cpp
	$(CC) -c $(CFLAGS) memory.cpp


clean:
	rm *.o
	rm ./emu