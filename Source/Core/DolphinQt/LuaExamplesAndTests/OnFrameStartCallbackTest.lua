

function mainFunction()
print("Main function still running at: " .. tostring(statistics:getCurrentFrame()))
if statistics:getCurrentFrame() >= 1500 then
	print("Finished at frame 1500!")
	OnFrameStart:unregister()
end
end

OnFrameStart:register(mainFunction)

