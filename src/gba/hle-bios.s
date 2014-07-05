#define nop andeq r0, r0

.text

b resetBase
b undefBase
b swiBase
b pabtBase
b dabtBase
nop
b irqBase
b fiqBase

resetBase:
mov pc, #0x8000000

swiBase:
cmp    sp, #0
moveq  sp, #0x04000000
subeq  sp, #0x20
stmfd  sp!, {r4-r5, lr}
ldrb   r4, [lr, #-2]
mov    r5, #swiTable
ldr    r4, [r5, r4, lsl #2]
cmp    r4, #0
mov    lr, pc
bxne   r4
ldmfd  sp!, {r4-r5, lr}
movs   pc, lr

swiTable:
.word SoftReset
.word RegisterRamReset
.word Halt
.word Stop
.word IntrWait
.word VBlankIntrWait
.word Div
.word DivArm
.word Sqrt
.word ArcTan
.word ArcTan2
.word CpuSet
.word CpuFastSet
# ... The rest of this table isn't needed if the rest aren't implemented

irqBase:
stmfd  sp!, {r0-r3, r12, lr}
mov    r0, #0x04000000
add    lr, pc, #0
ldr    pc, [r0, #-4]
ldmfd  sp!, {r0-r3, r12, lr}
subs   pc, lr, #4

VBlankIntrWait:
mov    r0, #1
mov    r1, #1
IntrWait:
stmfd  sp!, {r2-r3, lr}
mrs    r5, spsr
msr    cpsr, #0x1F
# Pull current interrupts enabled and add the ones we need
mov    r4, #0x04000000
# See if we want to return immediately
cmp    r0, #0
mov    r0, #0
mov    r2, #1
beq    1f
# Halt
0:
strb   r0, [r4, #0x301]
1:
# Check which interrupts were acknowledged
strb   r0, [r4, #0x208]
ldrh   r3, [r4, #-8]
ands   r3, r1
eorne  r3, r1
strneh r3, [r4, #-8]
strb   r2, [r4, #0x208]
beq    0b
msr    cpsr, #0x93
msr    spsr, r5
ldmfd  sp!, {r2-r3, pc}

CpuSet:
stmfd  sp!, {lr}
mrs    r5, spsr
msr    cpsr, #0x1F
mov    r3, r2, lsl #12
mov    r3, r3, lsr #12
tst    r2, #0x01000000
beq    0f
# Fill
tst    r2, #0x04000000
beq    1f
# Word
ldmia  r0!, {r2}
2:
stmia  r1!, {r2}
subs   r3, #1
bne    2b
b      3f
# Halfword
1:
bic    r0, #1
bic    r1, #1
ldrh   r2, [r0]
2:
strh   r2, [r1], #2
subs   r3, #1
bne    2b
b      3f
# Copy
0:
tst    r2, #0x04000000
beq    1f
# Word
2:
ldmia  r0!, {r2}
stmia  r1!, {r2}
subs   r3, #1
bne    2b
b      3f
# Halfword
1:
bic    r0, #1
bic    r1, #1
2:
ldrh   r2, [r0], #2
strh   r2, [r1], #2
subs   r3, #1
bne    2b
3:
msr    cpsr, #0x93
msr    spsr, r5
ldmfd  sp!, {pc}

CpuFastSet:
stmfd  sp!, {lr}
mrs    r5, spsr
msr    cpsr, #0x1F
stmfd  sp!, {r4-r10}
tst    r2, #0x01000000
mov    r3, r2, lsl #12
mov    r2, r3, lsr #12
beq    0f
# Fill
ldmia  r0!, {r4}
mov    r5, r4
mov    r3, r4
mov    r6, r4
mov    r7, r4
mov    r8, r4
mov    r9, r4
mov    r10, r4
1:
stmia  r1!, {r3-r10}
subs   r2, #8
bgt    1b
b      2f
# Copy
0:
ldmia  r0!, {r3-r10}
stmia  r1!, {r3-r10}
subs   r2, #8
bgt    0b
2:
ldmfd  sp!, {r4-r10}
msr    cpsr, #0x93
msr    spsr, r5
ldmfd  sp!, {pc}
