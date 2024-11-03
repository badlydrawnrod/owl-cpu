clang -Os --target=riscv32 -march=rv32i -mabi=ilp32 -ffreestanding -nostdlib -nodefaultlibs ./source/crt0.S ./source/fortune-owl.c -fuse-ld=lld "-Wl,--gc-sections" -T./source/basic.ld -o out/a.out
llvm-objcopy out/a.out -O binary out/a.bin -j .init -j .text -j .rodata -j .sdata -j .data
