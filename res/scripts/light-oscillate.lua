local theta = 0

function readLight()
	theta = math.fmod(theta + math.pi / 120, math.pi * 2)
	return (math.sin(theta) + 1) * 75
end

emu:setSolarSensorCallback(readLight)
