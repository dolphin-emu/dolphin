require ("type_resolver")
dolphin:importModule("RegistersAPI", "1.0.0")

RegistersClass = {}

function RegistersClass:new(obj)
	obj = obj or {}
	self.__index = self
	return setmetatable(obj, self)
end

registers = RegistersClass:new(nil)

RegisterNameMappingsTable = {
R0 = "R0",
R1 = "R1",
R2 = "R2",
R3 = "R3",
R4 = "R4",
R5 = "R5",
R6 = "R6",
R7 = "R7",
R8 = "R8",
R9 = "R9",
R10 = "R10",
R11 = "R11",
R12 = "R12",
R13 = "R13",
R14 = "R14",
R15 = "R15",
R16 = "R16",
R17 = "R17",
R18 = "R18",
R19 = "R19",
R20 = "R20",
R21 = "R21",
R22 = "R22",
R23 = "R23",
R24 = "R24",
R25 = "R25",
R26 = "R26",
R27 = "R27",
R28 = "R28",
R29 = "R29",
R30 = "R30",
R31 = "R31",
F0 = "F0",
F1 = "F1",
F2 = "F2",
F3 = "F3",
F4 = "F4",
F5 = "F5",
F6 = "F6",
F7 = "F7",
F8 = "F8",
F9 = "F9",
F10 = "F10",
F11 = "F11",
F12 = "F12",
F13 = "F13",
F14 = "F14",
F15 = "F15",
F16 = "F16",
F17 = "F17",
F18 = "F18",
F19 = "F19",
F20 = "F20",
F21 = "F21",
F22 = "F22",
F23 = "F23",
F24 = "F24",
F25 = "F25",
F26 = "F26",
F27 = "F27",
F28 = "F28",
F29 = "F29",
F30 = "F30",
F31 = "F31",
PC = "PC",
LR = "LR"
}

registers_get_register_table = {
[NumberTypes.U8] = function (registerString, offset) return RegistersAPI:getU8FromRegister(registerString, offset) end,
[NumberTypes.U16] = function (registerString, offset) return RegistersAPI:getU16FromRegister(registerString, offset) end,
[NumberTypes.U32] = function (registerString, offset) return RegistersAPI:getU32FromRegister(registerString, offset) end,
[NumberTypes.U64] = function (registerString, offset) return RegistersAPI:getU64FromRegister(registerString, offset) end,
[NumberTypes.S8] = function (registerString, offset) return RegistersAPI:getS8FromRegister(registerString, offset) end,
[NumberTypes.S16] = function (registerString, offset) return RegistersAPI:getS16FromRegister(registerString, offset) end,
[NumberTypes.S32] = function (registerString, offset) return RegistersAPI:getS32FromRegister(registerString, offset) end,
[NumberTypes.S64] = function (registerString, offset) return RegistersAPI:getS64FromRegister(registerString, offset) end,
[NumberTypes.FLOAT] = function (registerString, offset) return RegistersAPI:getFloatFromRegister(registerString, offset) end,
[NumberTypes.DOUBLE] = function (registerString, offset) return RegistersAPI:getDoubleFromRegister(registerString, offset) end
}



function RegistersClass:getRegister(registerString, returnTypeString, optionalOffset)
	if optionalOffset == nil then
		optionalOffset = 0
	elseif optionalOffset < 0 or optionalOffset >= 16 then
		error("Error: Offset passed into getRegister() was outside the maximum allowable range of 0-15!")
	end
	
	registerString = string.upper(registerString)
	
	registerEnum = RegisterNameMappingsTable[registerString]
	
	if (registerEnum == nil) then
		error("Error: Invalid register name passed into getRegister() function. Valid registers are R0-R31, F0-F31, PC and LR")
	end
	
	returnTypeEnum = ParseType(returnTypeString)
	
	if returnTypeEnum == nil then
		error("Error: Invalid typename passed into registers:getRegister(). Valid values include U8, U16, U32, U64, S8, S16, S32, S64, FLOAT, DOUBLE, UnsignedByte, SignedByte, UnsignedInt, SignedInt and SignedLongLong")
	end
	get_register_function_call = registers_get_register_table[returnTypeEnum]
	
	if get_register_function_call == nil then
		error("Error: An unknown implementation error occured in registers:getRegister()")
	end
	
	return get_register_function_call(registerString, optionalOffset)
end


function RegistersClass:getRegisterAsUnsignedByteArray(registerString, arraySize, optionalOffset)
	if optionalOffset == nil then
		optionalOffset = 0
	elseif optionalOffset < 0 or optionalOffset >= 16 then
		error("Error: Offset passed into getRegisterAsUnsignedByteArray() was outside the maximum allowable range of 0-15!")
	end
	
	registerString = string.upper(registerString)
	
	registerEnum = RegisterNameMappingsTable[registerString]
	
	if registerEnum == nil then
		error("Error: Invalid register name passed into getRegisterAsUnsignedByteArray(). Valid registers are R0-R31, F0-F31, PC and LR")
	end
	
	if arraySize <= 0 or arraySize > 16 then
		error("Error: Array size was outside the maximum allowable range of 1-16!")
	end
	
	return RegistersAPI:getUnsignedBytesFromRegister(registerString, arraySize, optionalOffset)
end


function RegistersClass:getRegisterAsSignedByteArray(registerString, arraySize, optionalOffset)
	if optionalOffset == nil then
		optionalOffset = 0
	elseif optionalOffset < 0 or optionalOffset >= 16 then
		error("Error: Offset passed into getRegisterAsSignedByteArray() was outside the maximum allowable range of 0-15!")
	end
	
	registerString = string.upper(registerString)
	
	registerEnum = RegisterNameMappingsTable[registerString]
	
	if registerEnum == nil then
		error("Error: Invalid register name passed into getRegisterAsSignedByteArray(). Valid registers are R0-R31, F0-F31, PC and LR")
	end
	
	if arraySize <= 0 or arraySize > 16 then
		error("Error: Array size was outside the maximum allowable range of 1-16!")
	end
	
	return RegistersAPI:getSignedBytesFromRegister(registerString, arraySize, optionalOffset)
end


registers_set_register_table = {
[NumberTypes.U8] = function (registerString, value, offset) return RegistersAPI:writeU8ToRegister(registerString, value, offset) end,
[NumberTypes.U16] = function (registerString, value, offset) return RegistersAPI:writeU16ToRegister(registerString, value, offset) end,
[NumberTypes.U32] = function (registerString, value, offset) return RegistersAPI:writeU32ToRegister(registerString, value, offset) end,
[NumberTypes.U64] = function (registerString, value, offset) return RegistersAPI:writeU64ToRegister(registerString, value, offset) end,
[NumberTypes.S8] = function (registerString, value, offset) return RegistersAPI:writeS8ToRegister(registerString, value, offset) end,
[NumberTypes.S16] = function (registerString, value, offset) return RegistersAPI:writeS16ToRegister(registerString, value, offset) end,
[NumberTypes.S32] = function (registerString, value, offset) return RegistersAPI:writeS32ToRegister(registerString, value, offset) end,
[NumberTypes.S64] = function (registerString, value, offset) return RegistersAPI:writeS64ToRegister(registerString, value, offset) end,
[NumberTypes.FLOAT] = function (registerString, value, offset) return RegistersAPI:writeFloatToRegister(registerString, value, offset) end,
[NumberTypes.DOUBLE] = function (registerString, value, offset) return RegistersAPI:writeDoubleToRegister(registerString, value, offset) end
}


function RegistersClass:setRegister(registerString, writeType, writeValue, optionalOffset)
	if optionalOffset == nil then
		optionalOffset = 0
	elseif optionalOffset < 0 or optionalOffset >= 16 then
		error("Error: Offset passed into setRegister() was outside the maximum allowable range of 0-15!")
	end
	
	registerString = string.upper(registerString)
	
	registerEnum = RegisterNameMappingsTable[registerString]
	
	if (registerEnum == nil) then
		error("Error: Invalid register name passed into setRegister() function. Valid registers are R0-R31, F0-F31, PC and LR")
	end
	
	returnTypeEnum = ParseType(writeType)
	
	if returnTypeEnum == nil then
		error("Error: Invalid typename passed into registers:setRegister(). Valid values include U8, U16, U32, U64, S8, S16, S32, S64, FLOAT, DOUBLE, UnsignedByte, SignedByte, UnsignedInt, SignedInt and SignedLongLong")
	end
	
	set_register_function_call = registers_set_register_table[returnTypeEnum]
	
	if set_register_function_call == nil then
		error("Error: An unknown implementation error occured in registers:setRegister()")
	end
	
	set_register_function_call(registerString, writeValue, optionalOffset)
end


function RegistersClass:setRegisterFromByteArray(registerString, indexToByteTable, optionalOffset)
	if optionalOffset == nil then
		optionalOffset = 0
	elseif optionalOffset < 0 or optionalOffset >= 16 then
		error("Error: Offset passed into setRegisterFromByteArray() was outside the maximum allowable range of 0-15!")
	end
	
	registerString = string.upper(registerString)
	
	registerEnum = RegisterNameMappingsTable[registerString]
	
	if (registerEnum == nil) then
		error("Error: Invalid register name passed into setRegisterFromByteArray() function. Valud registers are R0-R31, F0-F31, PC and LR")
	end
	
	if indexToByteTable == nil then
		error("Error: In setRegisterFromByteArray(), the indexToByteTable was nil!")
	end
	
	RegistersAPI:writeBytesToRegister(registerString, indexToByteTable, optionalOffset)
end