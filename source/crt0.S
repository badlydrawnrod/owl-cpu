    .section .init
    .global _start
    .type   _start, @function

_start:
    .cfi_startproc
    .cfi_undefined ra

    # Set the global pointer.
.option push
.option norelax
    la      gp, __global_pointer$
.option pop

    # Set the stack pointer.
    la      sp, __stack

#if USE_FLASH
    # Copy initialised data into RAM.
    la      a1, __data_lma_start__
    la      a2, __data_start__
    la      a3, __data_end__
    j       data_fill_test
data_fill_loop:
    lw      a0, (a1)
    sw      a0, (a2)
    addi    a1, a1, 4
    addi    a2, a2, 4
data_fill_test:
    bne     a2, a3, data_fill_loop
#endif

    # Clear BSS.
    la      a1, __bss_start__
    la      a2, __bss_end__
    j       bss_fill_test
bss_fill_loop:
    sw      zero, (a1)
    addi    a1, a1, 4
bss_fill_test:
    bne     a1, a2, bss_fill_loop

    # Call main().
run_main:
    li		a0, 0       # a0 = argc = 0
    li		a1, 0		# a1 = argv = NULL
    li		a2, 0		# a2 = envp = NULL
    call    main

    # Exit.
                        # a0 = return code from main()
    li      a7, 0       # a7 = syscall number (0 is exit)
    ecall               # do a syscall. There's no coming back from this one.

    .cfi_endproc

    .size  _start, .-_start

    .end
