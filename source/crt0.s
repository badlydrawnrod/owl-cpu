    .section .init
    .global _start
    .type   _start, @function

_start:
    .cfi_startproc
    .cfi_undefined ra

    # Set the global pointer.
.option push
.option norelax
    la gp, __global_pointer$
.option pop

    # TODO: set the stack pointer.

    # Copy initialised data into RAM.
    call    init_vma

    # Call main().
    li		a0, 0       # a0 = argc = 0
    li		a1, 0		# a1 = argv = NULL
    li		a2, 0		# a2 = envp = NULL
    call    main

    # Exit.
    li      a7, 0       # a7 = syscall number (0 is exit)
    ecall               # do a syscall. There's no coming back from this one.

    .cfi_endproc

    .size  _start, .-_start

    .end
