# Copyright (c) 2013-2014 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
mov r0, #0x8000000
ldrb r1, [r0, #3]
cmp r1, #0xEA
movne r0, #0x2000000
bx r0
.word 0
.word 0xE129F000

swiBase:
cmp    sp, #0
moveq  sp, #0x04000000
subeq  sp, #0x20
stmfd  sp!, {r11-r12, lr}
ldrb   r11, [lr, #-2]
mov    r12, #swiTable
ldr    r11, [r12, r11, lsl #2]
cmp    r11, #0
mrs    r12, spsr
stmfd  sp!, {r12}
and    r12, #0x80
orr    r12, #0x1F
msr    cpsr, r12
stmfd  sp!, {lr}
mov    lr, pc
bxne   r11
ldmfd  sp!, {lr}
msr    cpsr, #0x93
ldmfd  sp!, {r12}
msr    spsr, r12
ldmfd  sp!, {r11-r12, lr}
movs   pc, lr
.word 0
.word 0xE3A02004

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
.word 0
.word 0xE55EC002

VBlankIntrWait:
mov    r0, #1
mov    r1, #1
IntrWait:
stmfd  sp!, {r2-r3, lr}
mov    r12, #0x04000000
# See if we want to return immediately
cmp    r0, #0
mov    r0, #0
mov    r2, #1
beq    1f
ldrh   r3, [r12, #-8]
bic    r3, r1
strh   r3, [r12, #-8]
# Halt
0:
strb   r0, [r12, #0x301]
1:
# Check which interrupts were acknowledged
strb   r0, [r12, #0x208]
ldrh   r3, [r12, #-8]
ands   r3, r1
eorne  r3, r1
strneh r3, [r12, #-8]
strb   r2, [r12, #0x208]
beq    0b
ldmfd  sp!, {r2-r3, pc}

CpuSet:
stmfd  sp!, {lr}
mov    r3, r2, lsl #12
tst    r2, #0x01000000
beq    0f
# Fill
tst    r2, #0x04000000
beq    1f
# Word
add    r3, r1, r3, lsr #10
ldmia  r0!, {r2}
2:
cmp    r1, r3
stmltia  r1!, {r2}
blt    2b
b      3f
# Halfword
1:
bic    r0, #1
bic    r1, #1
add    r3, r1, r3, lsr #11
ldrh   r2, [r0]
2:
cmp    r1, r3
strlth r2, [r1], #2
blt    2b
b      3f
# Copy
0:
tst    r2, #0x04000000
beq    1f
# Word
add    r3, r1, r3, lsr #10
2:
cmp    r1, r3
ldmltia r0!, {r2}
stmltia r1!, {r2}
blt    2b
b      3f
# Halfword
1:
add    r3, r1, r3, lsr #11
2:
cmp    r1, r3
ldrlth r2, [r0], #2
strlth r2, [r1], #2
blt    2b
3:
ldmfd  sp!, {pc}

CpuFastSet:
stmfd  sp!, {r4-r10, lr}
tst    r2, #0x01000000
mov    r3, r2, lsl #12
add    r2, r1, r3, lsr #10
beq    0f
# Fill
ldr    r3, [r0]
mov    r4, r3
mov    r5, r3
mov    r6, r3
mov    r7, r3
mov    r8, r3
mov    r9, r3
mov    r10, r3
1:
cmp    r1, r2
stmltia r1!, {r3-r10}
blt    1b
b      2f
# Copy
0:
cmp    r1, r2
ldmltia r0!, {r3-r10}
stmltia r1!, {r3-r10}
blt    0b
2:
ldmfd  sp!, {r4-r10, pc}
