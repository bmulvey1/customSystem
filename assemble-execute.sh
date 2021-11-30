cd ./assembler && customasm ./main.casm
cd ../emu && make
sleep 2
./emu ../assembler/main.bin
