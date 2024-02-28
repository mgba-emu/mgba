local r = 0
local theta = 0
local rotation = {}

math.randomseed()

function rotation.sample()
	theta = math.fmod(theta + math.random() / 20, math.pi * 2)
	r = math.min(math.max(r + (math.random() - 0.5) / 50, -1), 1)
end

function rotation.readTiltX()
	return math.cos(theta) * r
end

function rotation.readTiltY()
	return math.sin(theta) * r
end

emu:setRotationCallbacks(rotation)
