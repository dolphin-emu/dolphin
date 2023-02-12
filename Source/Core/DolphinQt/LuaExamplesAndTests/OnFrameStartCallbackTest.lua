funcRef = 0

function mainFunction()
	print("Main function still running once per frame at frame : " .. tostring(statistics:getCurrentFrame()))
	if statistics:getCurrentFrame() >= 1500 then
		print("Finished main function at frame 1500!")
		OnFrameStart:unregister()
	end
end

function constantFunction()
	print("In the constant function")
end

OnFrameStart:register(mainFunction)
funcRef = Whenever:register(constantFunction)

