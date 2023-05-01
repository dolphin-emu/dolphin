dolphin:importModule("EmuAPI", "1.0")
dolphin:importModule("MemoryAPI", "1.0")
var1 = 0
funcRef = -1

function mainLoop()
 if var1 >= 20 then
	x = MemoryAPI:WriteAllMemoryAsUnsignedBytesToFile("tempFile.txt")
	OnFrameStart:unregister(funcRef)
end
var1 = var1 + 1
end

funcRef = OnFrameStart:register(mainLoop)