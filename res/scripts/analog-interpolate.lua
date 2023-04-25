local state = {}
state.period = 4
state.phase = 0
state.x = 0
state.y = 0

function state.update()
	state.phase = state.phase + 1
	if state.phase == state.period then
		state.phase = 0
	end
	if state.phase == 0 then
		if input.activeGamepad then
			local x = input.activeGamepad.axes[1] / 30000
			local y = input.activeGamepad.axes[2] / 30000
			-- Map the circle onto a square, since we don't
			-- want to have a duty of 1/sqrt(2) on the angles
			local theta = math.atan(y, x)
			local r = math.sqrt(x * x + y * y)
			if theta < math.pi * -3 / 4 then
				r = -r / math.cos(theta)
			elseif theta < math.pi * -1 / 4 then
				r = -r / math.sin(theta)
			elseif theta < math.pi * 1 / 4 then
				r = r / math.cos(theta)
			elseif theta < math.pi * 3 / 4 then
				r = r / math.sin(theta)
			elseif theta < math.pi * 5 / 4 then
				r = -r / math.cos(theta)
			end
			state.x = math.cos(theta) * r
			state.y = math.sin(theta) * r
		else
			state.x = 0
			state.y = 0
		end
	end
end

function state.read()
	emu:clearKeys(0xF0)
	if math.floor(math.abs(state.x) * state.period) > state.phase then
		if state.x > 0 then
			emu:addKey(C.GB_KEY.RIGHT)
		else
			emu:addKey(C.GB_KEY.LEFT)
		end
	end
	if math.floor(math.abs(state.y) * state.period) > state.phase then
		if state.y > 0 then
			emu:addKey(C.GB_KEY.DOWN)
		else
			emu:addKey(C.GB_KEY.UP)
		end
	end

	-- The duty cycle approach can confuse menus and the like,
	-- so the POV hat setting should force a direction on
	if input.activeGamepad and #input.activeGamepad.hats > 0 then
		local hat = input.activeGamepad.hats[1]
		if hat & C.INPUT_DIR.UP ~= 0 then
			emu:addKey(C.GB_KEY.UP)
		end
		if hat & C.INPUT_DIR.DOWN ~= 0 then
			emu:addKey(C.GB_KEY.DOWN)
		end
		if hat & C.INPUT_DIR.LEFT ~= 0 then
			emu:addKey(C.GB_KEY.LEFT)
		end
		if hat & C.INPUT_DIR.RIGHT ~= 0 then
			emu:addKey(C.GB_KEY.RIGHT)
		end
	end
end

callbacks:add("frame", state.update)
callbacks:add("keysRead", state.read)
