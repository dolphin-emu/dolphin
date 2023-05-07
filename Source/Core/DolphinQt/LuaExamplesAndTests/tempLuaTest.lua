dolphin:importModule("EmuAPI", "1.0")
dolphin:importModule("InstructionStepAPI", "1.0")
dolphin:importModule("RegistersAPI", "1.0")

function mainFunc()
	print(InstructionStepAPI:getInstructionFromAddress(RegistersAPI:getU32FromRegister("PC", 0) + 4))
	dolphin:shutdownScript()
end

OnInstructionHit:register(  RegistersAPI:getU32FromRegister("PC", 0), mainFunc)