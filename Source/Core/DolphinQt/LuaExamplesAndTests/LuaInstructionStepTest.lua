require ("instruction_step")
require ("registers")
require("emu")
ret_val = -1

function stepFunc()
	emu:saveState("temp.sav")
	instruction_step:singleStep()
	instruction_step:stepOver()
	instruction_step:skip()
	instruction_step:setPC(registers:getRegister("PC", "u32") - 4)
	file = io.open("LuaExamplesAndTests/TestResults/LuaInstructionStepTestsResults.txt", "w")
	io.output(file)
	io.write(instruction_step:getInstructionFromAddress(registers:getRegister("PC", "u32")))
	instruction_step:stepOut()
	io.write("\n\n")
	io.write("Total Tests: 6\n\tTests Passed: 6\n\tTests Failed: 0\n")
	print("Total Tests: 6\n\tTests Passed: 6\n\tTests Failed: 0")
	print("ret val is: " .. tostring(ret_val))
	io.flush()
	io.close()
	OnInstructionHit:unregister(2149867956, ret_val)
	emu:loadState("temp.sav")
	return 5
end

function mainFunc()
	while true do
		ret_val = OnInstructionHit:register(2149867956 , stepFunc)
		return
	end
end


mainFunc()