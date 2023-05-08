dolphin:importModule("InstructionStepAPI", "1.0")

InstructionStepClass = {}

function InstructionStepClass:new(obj)
	obj = obj or {}
	self.__index = self
	return setmetatable(obj, self)
end

instruction_step = InstructionStepClass:new(nil)

function InstructionStepClass:singleStep()
	InstructionStepAPI:singleStep()
end

function InstructionStepClass:stepOver()
	InstructionStepAPI:stepOver()
end

function InstructionStepClass:stepOut()
	InstructionStepAPI:stepOut()
end

function InstructionStepClass:skip()
	InstructionStepAPI:skip()
end

function InstructionStepClass:setPC(newPCValue)
	InstructionStepAPI:setPC(newPCValue)
end

function InstructionStepClass:getInstructionFromAddress(address)
	return InstructionStepAPI:getInstructionFromAddress(address)
end