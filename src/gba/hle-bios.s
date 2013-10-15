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
stmfd  sp!, {r2, lr}
ldrb   r2, [lr, #-2]
cmp    r2, #4
bleq   IntrWait
cmp    r2, #5
moveq  r0, #1
moveq  r1, #1
bleq   IntrWait
ldmfd  sp!, {r2, lr}
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
# Save inputs
mrs    r3, cpsr
bic    r3, #0x80
msr    cpsr, r3
# Pull current interrupts enabled and add the ones we need
mov    r4, #0x04000000
# See if we want to return immediately
cmp    r0, #0
mov    r0, #0
mov    r2, #1
beq    .L1
# Halt
.L0:
strb   r0, [r4, #0x301]
.L1:
# Check which interrupts were acknowledged
strb   r2, [r4, #0x204]
ldrh   r3, [r4, #-8]
ands   r3, r1
eorne  r3, r1
strneh r3, [r4, #-8]
strb   r0, [r4, #0x204]
beq .L0
#Restore state
mrs    r0, cpsr
orr    r0, #0x80
msr    cpsr, r0
ldmfd  sp!, {r4, pc}
