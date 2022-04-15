cd compiler && make clean && make && ./mcc source.txt ../assembler/main.asm
cd ../assembler && customasm ./main.asm
sleep 1
cd ../emu && make
./emu ../assembler/main.bin

