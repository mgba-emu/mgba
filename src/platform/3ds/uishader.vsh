; Copyright (c) 2015 Yuri Kunde Schlesner
;
; This Source Code Form is subject to the terms of the Mozilla Public
; License, v. 2.0. If a copy of the MPL was not distributed with this
; file, You can obtain one at http://mozilla.org/MPL/2.0/.

; uishader.vsh - Simply multiplies input position and texcoords with
;                corresponding matrices before outputting

; Uniforms
.fvec projectionMtx[4]
.fvec textureMtx[2]

; Constants
.constf consts1(0.0, 1.0, 0.0039215686, 0.0)

; Outputs : here only position and color
.out out_pos position
.out out_tc0 texcoord0
.out out_col color

; Inputs : here we have only vertices
.alias in_pos v0
.alias in_tc0 v1
.alias in_col v2

.proc main
	dp4 out_pos.x, projectionMtx[0], in_pos
	dp4 out_pos.y, projectionMtx[1], in_pos
	dp4 out_pos.z, projectionMtx[2], in_pos
	dp4 out_pos.w, projectionMtx[3], in_pos

	dp4 out_tc0.x, textureMtx[0], in_tc0
	dp4 out_tc0.y, textureMtx[1], in_tc0
	mov out_tc0.zw, consts1.xxxy

	; Normalize color by multiplying by 1 / 255
	mul out_col, consts1.z, in_col

	end
.end
