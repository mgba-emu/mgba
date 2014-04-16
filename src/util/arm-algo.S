# r0: Destination
# r1: Source
# r2: Number of words to copy as halfwords
.global _to16Bit
_to16Bit:
push {r4-r10}
mov r8, r0
mov r9, r1
mov r10, r2
.L0:
tst r10, #7
beq .L1
ldr r0, [r9], #4
strh r0, [r8], #2
sub r10, #1
b .L0
.L1:
ldmia r9!, {r0-r7}
strh r0, [r8], #2
strh r1, [r8], #2
strh r2, [r8], #2
strh r3, [r8], #2
strh r4, [r8], #2
strh r5, [r8], #2
strh r6, [r8], #2
strh r7, [r8], #2
subs r10, #8
bne .L1
.L9:
pop {r4-r10}
bx lr
