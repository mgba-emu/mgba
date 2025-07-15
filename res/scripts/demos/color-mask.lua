local state = {}
state.wheel = image.load(script.dir .. "/wheel.png")
state.overlay = canvas:newLayer(state.wheel.width, state.wheel.height)
state.painter = image.newPainter(state.overlay.image)
state.phase = 0
state.speed = 0.01
state.painter:setFill(true)
state.painter:setStrokeWidth(0)

function state.update()
	local r = math.fmod(state.phase * 3, math.pi * 2)
	local g = math.fmod(state.phase * 5, math.pi * 2)
	local b = math.fmod(state.phase * 7, math.pi * 2)
	local color = 0xFF000000
	color = color | math.floor((math.sin(r) + 1) * 127.5) << 16
	color = color | math.floor((math.sin(g) + 1) * 127.5) << 8
	color = color | math.floor((math.sin(b) + 1) * 127.5)

	-- Clear image
	state.painter:setBlend(false)
	state.painter:setFillColor(0)
	state.painter:drawRectangle(0, 0, state.wheel.width, state.wheel.height)
	-- Draw mask
	state.painter:setBlend(true)
	state.painter:setFillColor(color | 0xFF000000)
	state.painter:drawMask(state.wheel, 0, 0)

	state.overlay:update()
	state.phase = math.fmod(state.phase + state.speed, math.pi * 2 * 3 * 5 * 7)
end

callbacks:add("frame", state.update)
