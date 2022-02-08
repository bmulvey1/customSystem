cd assembler && customasm ./main.asm
cd ../emu && make
sleep 2
./emu ../assembler/main.bin

