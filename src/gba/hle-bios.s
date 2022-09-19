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

.word 0 @ Padding for back-compat
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0
.word 0

swiBase:
cmp    sp, #0
moveq  sp, #0x04000000
subeq  sp, #0x20
stmfd  sp!, {r11, r12, lr}
ldrb   r12, [lr, #-2]
mov    r11, #swiTable
ldr    r12, [r11, r12, lsl #2]
mov    r11, #StallCall
cmp    r11, r12
mrs    r11, spsr
stmfd  sp!, {r11}
and    r11, #0x80
orr    r11, #0x1F
msr    cpsr_c, r11
swieq  0xF00000  @ Special mGBA-internal call to load the stall count into r11
stmfd  sp!, {r2, lr}
cmp    r12, #0
nop
nop
nop
nop
nop
nop
movne  lr, pc
bxne   r12
nop
nop
nop
ldmfd  sp!, {r2, lr}
msr    cpsr, #0x93
ldmfd  sp!, {r11}
msr    spsr, r11
ldmfd  sp!, {r11, r12, lr}
movs   pc, lr
.word 0
.word 0xE3A02004

.word 0 @ Padding for back-compat

swiTable:
.word SoftReset               @ 0x00
.word RegisterRamReset        @ 0x01
.word Halt                    @ 0x02
.word Stop                    @ 0x03
.word IntrWait                @ 0x04
.word VBlankIntrWait          @ 0x05
.word Div                     @ 0x06
.word DivArm                  @ 0x07
.word Sqrt                    @ 0x08
.word ArcTan                  @ 0x09
.word ArcTan2                 @ 0x0A
.word CpuSet                  @ 0x0B
.word CpuFastSet              @ 0x0C
.word GetBiosChecksum         @ 0x0D
.word BgAffineSet             @ 0x0E
.word ObjAffineSet            @ 0x0F
.word BitUnPack               @ 0x10
.word Lz77UnCompWram          @ 0x11
.word Lz77UnCompVram          @ 0x12
.word HuffmanUnComp           @ 0x13
.word RlUnCompWram            @ 0x14
.word RlUnCompVram            @ 0x15
.word Diff8BitUnFilterWram    @ 0x16
.word Diff8BitUnFilterVram    @ 0x17
.word Diff16BitUnFilter       @ 0x18
.word SoundBias               @ 0x19
.word SoundDriverInit         @ 0x1A
.word SoundDriverMode         @ 0x1B
.word SoundDriverMain         @ 0x1C
.word SoundDriverVsync        @ 0x1D
.word SoundChannelClear       @ 0x1E
.word MidiKey2Freq            @ 0x1F
.word MusicPlayerOpen         @ 0x20
.word MusicPlayerStart        @ 0x21
.word MusicPlayerStop         @ 0x22
.word MusicPlayerContinue     @ 0x23
.word MusicPlayerFadeOut      @ 0x24
.word MultiBoot               @ 0x25
.word HardReset               @ 0x26
.word CustomHalt              @ 0x27
.word SoundDriverVsyncOff     @ 0x28
.word SoundDriverVsyncOn      @ 0x29
.word SoundDriverGetJumpList  @ 0x2A

.ltorg
.word 0 @ Padding for back-compat

irqBase:
stmfd  sp!, {r0-r3, r12, lr}
mov    r0, #0x04000000
add    lr, pc, #0
ldr    pc, [r0, #-4]
ldmfd  sp!, {r0-r3, r12, lr}
subs   pc, lr, #4
.word 0
.word 0xE55EC002

undefBase:
subs   pc, lr, #4
.word 0
.word 0x03A0E004

@ Unimplemented
SoftReset:
RegisterRamReset:
Stop:
GetBiosChecksum:
BgAffineSet:
ObjAffineSet:
BitUnPack:
HuffmanUnComp:
RlUnCompWram:
RlUnCompVram:
Diff8BitUnFilterWram:
Diff8BitUnFilterVram:
Diff16BitUnFilter:
SoundBias:
SoundDriverInit:
SoundDriverMode:
SoundDriverMain:
SoundDriverVsync:
SoundChannelClear:
MidiKey2Freq:
MusicPlayerOpen:
MusicPlayerStart:
MusicPlayerStop:
MusicPlayerContinue:
MusicPlayerFadeOut:
MultiBoot:
HardReset:
CustomHalt:
SoundDriverVsyncOff:
SoundDriverVsyncOn:

NopCall:
bx lr

Halt:
mov    r11, #0
mov    r12, #0x04000000
strb   r11, [r12, #0x301]
bx     lr

VBlankIntrWait:
mov    r0, #1
mov    r1, #1
IntrWait:
stmfd  sp!, {r2-r3, lr}
mov    r12, #0x04000000
@ See if we want to return immediately
cmp    r0, #0
mov    r0, #0
mov    r2, #1
beq    1f
ldrh   r3, [r12, #-8]
bic    r3, r1
strh   r3, [r12, #-8]
@ Halt
0:
strb   r0, [r12, #0x301]
1:
@ Check which interrupts were acknowledged
strb   r0, [r12, #0x208]
ldrh   r3, [r12, #-8]
ands   r3, r1
eorne  r3, r1
strneh r3, [r12, #-8]
strb   r2, [r12, #0x208]
beq    0b
ldmfd  sp!, {r2-r3, pc}

CpuSet:
stmfd  sp!, {r4, r5, lr}
mov    r4, r2, lsl #12
mov    r12, r0
mov    r5, r1
tst    r2, #0x01000000
beq    0f
@ Fill
tst    r2, #0x04000000
beq    1f
@ Word
add    r4, r5, r4, lsr #10
ldmia  r12!, {r3}
2:
cmp    r5, r4
stmltia  r5!, {r3}
blt    2b
b      3f
@ Halfword
1:
bic    r12, #1
bic    r5, #1
add    r4, r5, r4, lsr #11
ldrh   r3, [r12]
2:
cmp    r5, r4
strlth r3, [r5], #2
blt    2b
b      3f
@ Copy
0:
tst    r2, #0x04000000
beq    1f
@ Word
add    r4, r5, r4, lsr #10
2:
cmp    r5, r4
ldmltia r12!, {r3}
stmltia r5!, {r3}
blt    2b
b      3f
@ Halfword
1:
add    r4, r5, r4, lsr #11
2:
cmp    r5, r4
ldrlth r3, [r12], #2
strlth r3, [r5], #2
blt    2b
3:
mov    r3, #0x170  @ Match official BIOS's clobbered r3
ldmfd  sp!, {r4, r5, pc}

CpuFastSet:
stmfd  sp!, {r4-r10, lr}
tst    r2, #0x01000000
mov    r3, r2, lsl #12
add    r2, r1, r3, lsr #10
beq    0f
@ Fill
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
@ Copy
0:
cmp    r1, r2
ldmltia r0!, {r3-r10}
stmltia r1!, {r3-r10}
blt    0b
2:
ldmfd  sp!, {r4-r10, pc}

SoundDriverGetJumpList:
stmfd  sp!, {r4-r10}
ldr    r1, =NopCall
mov    r3, r1
mov    r4, r1
mov    r5, r1
mov    r6, r1
mov    r7, r1
mov    r8, r1
mov    r9, r1
mov    r10, r1
stmia  r0!, {r1, r3-r10}
stmia  r0!, {r1, r3-r10}
stmia  r0!, {r1, r3-r10}
stmia  r0!, {r1, r3-r10}
mov    r1, #0
ldmfd  sp!, {r4-r10}
bx     lr

.ltorg

Div:
DivArm:
Sqrt:
ArcTan:
ArcTan2:
Lz77UnCompWram:
Lz77UnCompVram:

StallCall:
subs r11, #4
bhi StallCall
bx lr

resetBase:
mov lr, #0x8000003
ldrb r1, [lr], #-3
cmp r1, #0
movne r1, #0
bxne lr
ldr lr, =0x20000C0
ldr r1, [lr]
cmp r1, #0
mov r1, #0
bxne lr
sub lr, #0xC0
bx lr
.word 0
.word 0xE129F000
