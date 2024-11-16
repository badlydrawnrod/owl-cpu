if [[ "$#" -ne 1 ]]; then
    echo "Usage: $(basename $0) <filename>" >&2
    exit 2
fi

source=$1
clang -DUSE_FLASH -Os --target=riscv32 -march=rv32i -mabi=ilp32 -ffreestanding -nostdlib -nodefaultlibs ./source/crt0.S ./source/$source -fuse-ld=lld "-Wl,--gc-sections" -T./source/memory_flash.ld -T./source/sections.ld -o out/a.out
