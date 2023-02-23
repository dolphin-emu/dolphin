require ("statistics")

funcRef = 0
function mainFunction()
	print("Main function still running once per frame at frame: " .. tostring(statistics:getCurrentFrame()))
	if statistics:getCurrentFrame() >= 1500 then
		print("Finished main function at frame 1500!")
		OnFrameStart:unregister(funcRef)
	end
end

funcRef = OnFrameStart:register(mainFunction)
