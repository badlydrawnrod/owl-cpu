    .section .init
    .global _start
    .type   _start, @function

_start:
    .cfi_startproc
    .cfi_undefined ra

    # Call main().
    li		a0,0        # a0 = argc = 0
    li		a1,0		# a1 = argv = NULL
    li		a2,0		# a2 = envp = NULL
    call    main

    # Exit.
    li      a0,0        # a0 = exit code
    li      a7,0        # a7 = syscall number (0 is exit)
    ecall               # do a syscall. There's no coming back from this one.

    .cfi_endproc

    .size  _start, .-_start

    .end
