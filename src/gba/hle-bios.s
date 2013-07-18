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
stmfd  sp!, {lr}
ldrb   r0, [lr, #-2]
cmp    r0, #4
bleq   IntrWait
cmp    r0, #5
bleq   IntrWait
ldmfd  sp!, {lr}
movs   pc, lr

irqBase:
stmfd  sp!, {r0-r3, r12, lr}
mov    r0, #0x04000000
add    lr, pc, #0
ldr    pc, [r0, #-4]
ldmfd  sp!, {r0-r3, r12, lr}
subs   pc, lr, #4

IntrWait:
stmfd  sp!, {r4, lr}
add    sp, #-4
strh   r1, [sp, #0]
mov    r4, #0x04000000
add    r4, #0x200
ldrh   r0, [r4, #0]
strh   r0, [sp, #2]
ldrh   r1, [sp, #0]
orr    r0, r1
strh   r0, [r4, #0x0]
mov    r4, #0x04000000
IntrWaitLoop:
mov    r0, #0x1F
msr    cpsr, r0
mov    r0, #0
strb   r0, [r4, #0x301]
mov    r0, #0xD3
msr    cpsr, r0
ldrh   r0, [r4, #-8]
ldrh   r1, [sp, #0]
ands   r1, r0
eorne  r1, r0
strneh r1, [r4, #-8]
beq    IntrWaitLoop
mov    r4, #0x04000000
add    r4, #0x200
ldrh   r0, [sp, #2]
strh   r0, [r4, #0]
add    sp, #4
ldmfd  sp!, {r4, pc}
