math.randomseed(os.time())
local state = {}
state.logo = image.load(script.dir .. "/logo.png")
state.overlay = canvas:newLayer(state.logo.width, state.logo.height)
state.overlay.image:drawImageOpaque(state.logo, 0, 0)
state.x = math.random() * (canvas:screenWidth() - state.logo.width)
state.y = math.random() * (canvas:screenHeight() - state.logo.height)
state.direction = math.floor(math.random() * 3)
state.speed = 0.5

state.overlay:setPosition(math.floor(state.x), math.floor(state.y))
state.overlay:update()

function state.update()
	if state.direction & 1 == 1 then
		state.x = state.x + 1
		if state.x > canvas:screenWidth() - state.logo.width then
			state.x = (canvas:screenWidth() - state.logo.width) * 2 - state.x
			state.direction = state.direction ~ 1
		end
	else
		state.x = state.x - 1
		if state.x < 0 then
			state.x = -state.x
			state.direction = state.direction ~ 1
		end
	end
	if state.direction & 2 == 2 then
		state.y = state.y + 1
		if state.y > canvas:screenHeight() - state.logo.height then
			state.y = (canvas:screenHeight() - state.logo.height) * 2 - state.y
			state.direction = state.direction ~ 2
		end
	else
		state.y = state.y - 1
		if state.y < 0 then
			state.y = -state.y
			state.direction = state.direction ~ 2
		end
	end
	state.overlay:setPosition(math.floor(state.x), math.floor(state.y))
end

callbacks:add("frame", state.update)
