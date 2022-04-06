cd compiler && make clean && make && ./mcc source.txt ../assembler/main.asm
sleep 1
cd ../assembler && customasm ./main.asm
sleep 1
cd ../emu && make
sleep 1
./emu ../assembler/main.bin

