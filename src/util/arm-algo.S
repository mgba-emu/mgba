# Copyright (c) 2013-2015 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#if defined(__ARM_NEON) && !defined(PSP2)
# r0: Destination
# r1: Source
# r2: Width
# r3: Height
.global _neon2x
_neon2x:
push {r4-r5}
lsl r4, r2, #2
.n20:
mov r2, r4, lsr #4
add r5, r0, r4
.n21:
vld2.32 {d0[], d1[]}, [r1]!
vmov d2, d0
vmov d3, d1
vzip.16 d0, d2
vzip.16 d1, d3
vst1.32 {q0}, [r0]!
vst1.32 {q0}, [r5]!
subs r2, #1
bne .n21
subs r3, #1
mov r0, r5
bne .n20
pop {r4-r5}
bx lr

.global _neon4x
_neon4x:
push {r4-r7}
lsl r4, r2, #3
.n40:
mov r2, r4, lsr #5
add r5, r0, r4
add r6, r5, r4
add r7, r6, r4
.n41:
vld4.16 {d0[], d1[], d2[], d3[]}, [r1]!
vst1.16 {d0}, [r0]!
vst1.16 {d0}, [r5]!
vst1.16 {d0}, [r6]!
vst1.16 {d0}, [r7]!
vst1.16 {d1}, [r0]!
vst1.16 {d1}, [r5]!
vst1.16 {d1}, [r6]!
vst1.16 {d1}, [r7]!
vst1.16 {d2}, [r0]!
vst1.16 {d2}, [r5]!
vst1.16 {d2}, [r6]!
vst1.16 {d2}, [r7]!
vst1.16 {d3}, [r0]!
vst1.16 {d3}, [r5]!
vst1.16 {d3}, [r6]!
vst1.16 {d3}, [r7]!
subs r2, #1
bne .n41
subs r3, #1
mov r0, r7
bne .n40
pop {r4-r7}
bx lr
#endif

.section	.note.GNU-stack,"",%progbits
