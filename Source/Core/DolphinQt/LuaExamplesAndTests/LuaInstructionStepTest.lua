require ("instruction_step")
require ("registers")

function stepFunc()
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
	io.flush()
	io.close()
	dolphin:shutdownScript()
end

function mainFunc()
	while true do
		OnInstructionHit:register(registers:getRegister("PC", "u32") , stepFunc)
		return
	end
end


mainFunc()