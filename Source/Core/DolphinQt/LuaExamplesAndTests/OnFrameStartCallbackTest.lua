dolphin:importModule("statistics")

funcRef = 0

function mainFunction()
	print("Main function still running once per frame at frame: " .. tostring(statistics:getCurrentFrame()))
	if statistics:getCurrentFrame() >= 1500 then
		print("Finished main function at frame 1500!")
		OnFrameStart:unregister()
	end
end

function constantFunction()
	if statistics:getCurrentFrame() % 100 == 2 then
		print("In the constant function at frame: " .. tostring(statistics:getCurrentFrame()))
	end
	if statistics:getCurrentFrame() >= 1510 then
		print("Finished constant function at frame 1510")
		Whenever:unregister(funcRef)
	end
end

OnFrameStart:register(mainFunction)
