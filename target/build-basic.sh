#!/usr/bin/bash

if [[ "$#" -ne 1 ]]; then
    echo "Usage: $(basename $0) <filename>" >&2
    exit 2
fi

pushd $(dirname $0) > /dev/null

source=$1
clang -Os --target=riscv32 -march=rv32i -mabi=ilp32 -ffreestanding -nostdlib -nodefaultlibs ./source/crt0.S ./source/$source -fuse-ld=lld "-Wl,--gc-sections" -T./source/basic.ld -o out/a.out

popd > /dev/null
