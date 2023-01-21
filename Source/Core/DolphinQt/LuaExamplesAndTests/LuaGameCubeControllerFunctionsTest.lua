
file = io.open("LuaExamplesAndTests/TestResults/LuaGameCubeControllerTestsResults.txt", "w")
io.output(file)

emu:playMovie("movieWithAInputs.dtm")
emu:loadState("testEarlySaveState.sav")
while statistics:getCurrentFrame() < 1500 do
	emu:frameAdvance()
end

io.write(tostring(gc_controller:getControllerInputs(1)["A"]))
io.flush()
io.close()
emu:frameAdvance()
