input_display = {
	anchor = "top",
	offset = {
		x = 0,
		y = 0,
	}
}

local state = {
	drawButton = {
		[0] = function(state) -- A
			state.painter:drawCircle(27, 6, 4)
		end,
		[1] = function(state) -- B
			state.painter:drawCircle(23, 8, 4)
		end,
		[2] = function(state) -- Select
			state.painter:drawCircle(13, 11, 3)
		end,
		[3] = function(state) -- Start
			state.painter:drawCircle(18, 11, 3)
		end,
		[4] = function(state) -- Right
			state.painter:drawRectangle(9, 7, 4, 3)
		end,
		[5] = function(state) -- Left
			state.painter:drawRectangle(2, 7, 4, 3)
		end,
		[6] = function(state) -- Up
			state.painter:drawRectangle(6, 3, 3, 4)
		end,
		[7] = function(state) -- Down
			state.painter:drawRectangle(6, 10, 3, 4)
		end,
		[8] = function(state) -- R
			state.painter:drawRectangle(28, 0, 4, 3)
		end,
		[9] = function(state) -- L
			state.painter:drawRectangle(0, 0, 4, 3)
		end
	},
	maxKey = {
		[C.PLATFORM.GBA] = 9,
		[C.PLATFORM.GB] = 7,
	}
}

function state.create()
	if state.overlay ~= nil then
		return true
	end
	if canvas == nil then
		return false
	end
	state.overlay = canvas:newLayer(32, 16)
	if state.overlay == nil then
		return false
	end
	state.painter = image.newPainter(state.overlay.image)
	state.painter:setBlend(false)
	state.painter:setFill(true)
	return true
end

function state.update()
	local endX = canvas:screenWidth() - 32
	local endY = canvas:screenHeight() - 16

	local anchors = {
		topLeft = {
			x = 0,
			y = 0
		},
		top = {
			x = endX / 2,
			y = 0
		},
		topRight = {
			x = endX,
			y = 0
		},
		left = {
			x = 0,
			y = endY / 2
		},
		center = {
			x = endX / 2,
			y = endY / 2
		},
		right = {
			x = endX,
			y = endY / 2
		},
		bottomLeft = {
			x = 0,
			y = endY
		},
		bottom = {
			x = endX / 2,
			y = endY
		},
		bottomRight = {
			x = endX,
			y = endY
		},
	}

	local pos = anchors[input_display.anchor];
	pos.x = pos.x + input_display.offset.x;
	pos.y = pos.y + input_display.offset.y;

	state.overlay:setPosition(pos.x, pos.y);

	local keys = util.expandBitmask(emu:getKeys())
	local maxKey = state.maxKey[emu:platform()]

	for key = 0, maxKey do
		if emu:getKey(key) ~= 0 then
			state.painter:setFillColor(0x80FFFFFF)
		else
			state.painter:setFillColor(0x40404040)
		end
		state.drawButton[key](state)
	end
	state.overlay:update()
end

function state.reset()
	if not state.create() then
		return
	end
	state.painter:setFillColor(0x40808080)
	state.painter:drawRectangle(0, 0, 32, 16)
	state.overlay:update()
end

input_display.state = state

state.reset()
callbacks:add("frame", state.update)
callbacks:add("start", state.reset)
