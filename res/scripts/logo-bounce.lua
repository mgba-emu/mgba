math.randomseed(os.time())
local state = {}
state.logo_fg = image.load(script.dir .. "/logo-fg.png")
state.logo_bg = image.load(script.dir .. "/logo-bg.png")
state.overlay = canvas:newLayer(state.logo_fg.width, state.logo_fg.height)
state.x = math.random() * (canvas:screenWidth() - state.logo_fg.width)
state.y = math.random() * (canvas:screenHeight() - state.logo_fg.height)
state.overlay:setPosition(math.floor(state.x), math.floor(state.y))
state.direction = math.floor(math.random() * 3)
state.speed = 0.5

function state.recolor()
	local r = math.floor(math.random() * 255)
	local g = math.floor(math.random() * 255)
	local b = math.floor(math.random() * 255)
	local color = 0xFF000000 | (r << 16) | (g << 8) | b
	state.overlay.image:drawImageOpaque(state.logo_bg, 0, 0)
	local painter = image.newPainter(state.overlay.image)
	painter:setFill(true)
	painter:setFillColor(color)
	painter:setBlend(true)
	painter:drawMask(state.logo_fg, 0, 0)
	state.overlay:update()
end

function state.update()
	if state.direction & 1 == 1 then
		state.x = state.x + 1
		if state.x > canvas:screenWidth() - state.logo_fg.width then
			state.x = (canvas:screenWidth() - state.logo_fg.width) * 2 - state.x
			state.direction = state.direction ~ 1
			state.recolor()
		end
	else
		state.x = state.x - 1
		if state.x < 0 then
			state.x = -state.x
			state.direction = state.direction ~ 1
			state.recolor()
		end
	end
	if state.direction & 2 == 2 then
		state.y = state.y + 1
		if state.y > canvas:screenHeight() - state.logo_fg.height then
			state.y = (canvas:screenHeight() - state.logo_fg.height) * 2 - state.y
			state.direction = state.direction ~ 2
			state.recolor()
		end
	else
		state.y = state.y - 1
		if state.y < 0 then
			state.y = -state.y
			state.direction = state.direction ~ 2
			state.recolor()
		end
	end
	state.overlay:setPosition(math.floor(state.x), math.floor(state.y))
end

state.recolor()
callbacks:add("frame", state.update)
