; -O2 -march=rv32i -mabi=ilp32

main:
        ; Enter main(), saving the registers that it uses onto the stack.
        addi    sp, sp, -32
        sw      ra, 28(sp)
        sw      s0, 24(sp)
        sw      s1, 20(sp)
        sw      s2, 16(sp)
        sw      s3, 12(sp)
        sw      s4, 8(sp)
        ; ------------------------------------------------------------------------------------------
        li      s0, 0                       ; i = 0
        li      s2, 2                       ; s2 = 2
        lui     a0, %hi(format_str)
        addi    s1, a0, %lo(format_str)     ; s1 = the address of the printf format string
        li      s3, 48                      ; s3 = 48
        li      s4, 1                       ; s4 = 1
        j       fib                         ; go to fib
print_loop:
        mv      a0, s1                      ; arg0 = the address of the printf format string
        mv      a1, s0                      ; arg1 = i
                                            ; arg2 is already set to current
        call    printf                      ; call printf
        addi    s0, s0, 1                   ; i = i + 1
        beq     s0, s3, done                ; if i == 48 go to done
fib:
        mv      a2, s0                      ; current = i
        bltu    s0, s2, print_loop          ; if i < 2 go to print_loop
        li      a0, 0                       ; previous = 0
        li      a2, 1                       ; current = 1
        mv      a1, s0                      ; n = i
fib_loop:
        mv      a3, a2                      ; tmp = current
        addi    a1, a1, -1                  ; n = n - 1
        add     a2, a0, a2                  ; current = current + prev
        mv      a0, a3                      ; previous = tmp
        bltu    s4, a1, fib_loop            ; if n > 1 go to fib_loop
        j       print_loop                  ; go to print_loop
done:
        li      a0, 0                       ; set the return value of main() to 0
        ; ------------------------------------------------------------------------------------------
        ; Restore registers from the stack and return from main().
        lw      ra, 28(sp)
        lw      s0, 24(sp)
        lw      s1, 20(sp)
        lw      s2, 16(sp)
        lw      s3, 12(sp)
        lw      s4, 8(sp)
        addi    sp, sp, 32
        ret                                 ; return
format_str:
        .asciz  "fib(%u) = %u\n"            ; the printf format string, terminated by '\0'
