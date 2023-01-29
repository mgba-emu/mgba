inputBuffer = console:createBuffer("Input")

function readPad()
	inputBuffer:clear()

	if not input.activeGamepad then
		inputBuffer:print("No gamepad detected\n")
		return
	end

	local gamepad = input.activeGamepad
	local axes = gamepad.axes
	local buttons = gamepad.buttons
	local hats = gamepad.hats

	inputBuffer:print(gamepad.visibleName .. "\n")
	inputBuffer:print(string.format("%i buttons, %i axes, %i hats\n", #buttons, #axes, #hats))

	local sbuttons = {}
	for k, v in ipairs(buttons) do
		if v then
			sbuttons[k] = "down"
		else
			sbuttons[k] = "  up"
		end
	end

	inputBuffer:print(string.format("Buttons: %s\n", table.concat(sbuttons, ", ")))
	inputBuffer:print(string.format("Axes: %s\n", table.concat(axes, ", ")))
	inputBuffer:print(string.format("Hats: %s\n", table.concat(hats, ", ")))
end

callbacks:add("frame", readPad)
