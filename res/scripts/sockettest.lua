sockettest = nil
lastkeys = nil

local KEY_NAMES = { "A", "B", "s", "S", "<", ">", "^", "v", "R", "L" }

function ST_stop()
	if not sockettest then return end
	console:log("Socket Test: Shutting down")
	sockettest:close()
	sockettest = nil
end

function ST_start()
	ST_stop()
	console:log("Socket Test: Connecting to 127.0.0.1:8888...")
	sockettest = socket.tcp()
	sockettest:add("received", ST_received)
	sockettest:add("error", ST_error)
	if sockettest:connect("127.0.0.1", 8888) then
		console:log("Socket Test: Connected")
		lastkeys = nil
	else
		console:log("Socket Test: Failed to connect")
		ST_stop()
	end
end

function ST_error(err)
	console:error("Socket Test Error: " .. err)
	ST_stop()
end

function ST_received()
	while true do
		local p, err = sockettest:receive(1024)
		if p then
			console:log("Socket Test Received: " .. p:match("^(.-)%s*$"))
		else
			if err ~= socket.ERRORS.AGAIN then
				console:error("Socket Test Error: " .. err)
				ST_stop()
			end
			return
		end
	end
end

function ST_scankeys()
	if not sockettest then return end
	local keys = emu:getKeys()
	if keys ~= lastkeys then
		lastkeys = keys
		local msg = "["
		for i, k in ipairs(KEY_NAMES) do
			if (keys & (1 << (i - 1))) == 0 then
				msg = msg .. " "
			else
				msg = msg .. k;
			end
		end
		sockettest:send(msg .. "]\n")
	end
end

callbacks:add("start", ST_start)
callbacks:add("stop", ST_stop)
callbacks:add("crashed", ST_stop)
callbacks:add("reset", ST_start)
callbacks:add("keysRead", ST_scankeys)
