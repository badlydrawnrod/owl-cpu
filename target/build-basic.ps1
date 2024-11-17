param (
    [Parameter(Mandatory = $true)][string]$source
)

$old = $PWD
Set-Location $PSScriptRoot

try {
    New-Item -Path .\out -ItemType Directory -Force | Out-Null
    clang -Os --target=riscv32 -march=rv32i -mabi=ilp32 -ffreestanding -nostdlib -nodefaultlibs ./source/crt0.S ./source/$source -fuse-ld=lld "-Wl,--gc-sections" -T./source/basic.ld -o out/a.out

}
finally {
    Set-Location $old
}
