from type_resolver import NumberTypes
from type_resolver import ParseType
import dolphin
dolphin.importModule("RegistersAPI", "1.0.0")
import RegistersAPI


_INTERNAL_RegisterNameMappingsTable = {} #This table allows us to quickly check if a register name is defined or not.
_INTERNAL_RegisterNameMappingsTable["R0"] = "R0"
_INTERNAL_RegisterNameMappingsTable["R1"] = "R1"
_INTERNAL_RegisterNameMappingsTable["R2"] = "R2"
_INTERNAL_RegisterNameMappingsTable["R3"] = "R3"
_INTERNAL_RegisterNameMappingsTable["R4"] = "R4"
_INTERNAL_RegisterNameMappingsTable["R5"] = "R5"
_INTERNAL_RegisterNameMappingsTable["R6"] = "R6"
_INTERNAL_RegisterNameMappingsTable["R7"] = "R7"
_INTERNAL_RegisterNameMappingsTable["R8"] = "R8"
_INTERNAL_RegisterNameMappingsTable["R9"] = "R9"
_INTERNAL_RegisterNameMappingsTable["R10"] = "R10"
_INTERNAL_RegisterNameMappingsTable["R11"] = "R11"
_INTERNAL_RegisterNameMappingsTable["R12"] = "R12"
_INTERNAL_RegisterNameMappingsTable["R13"] = "R13"
_INTERNAL_RegisterNameMappingsTable["R14"] = "R14"
_INTERNAL_RegisterNameMappingsTable["R15"] = "R15"
_INTERNAL_RegisterNameMappingsTable["R16"] = "R16"
_INTERNAL_RegisterNameMappingsTable["R17"] = "R17"
_INTERNAL_RegisterNameMappingsTable["R18"] = "R18"
_INTERNAL_RegisterNameMappingsTable["R19"] = "R19"
_INTERNAL_RegisterNameMappingsTable["R20"] = "R20"
_INTERNAL_RegisterNameMappingsTable["R21"] = "R21"
_INTERNAL_RegisterNameMappingsTable["R22"] = "R22"
_INTERNAL_RegisterNameMappingsTable["R23"] = "R23"
_INTERNAL_RegisterNameMappingsTable["R24"] = "R24"
_INTERNAL_RegisterNameMappingsTable["R25"] = "R25"
_INTERNAL_RegisterNameMappingsTable["R26"] = "R26"
_INTERNAL_RegisterNameMappingsTable["R27"] = "R27"
_INTERNAL_RegisterNameMappingsTable["R28"] = "R28"
_INTERNAL_RegisterNameMappingsTable["R29"] = "R29"
_INTERNAL_RegisterNameMappingsTable["R30"] = "R30"
_INTERNAL_RegisterNameMappingsTable["R31"] = "R31"
_INTERNAL_RegisterNameMappingsTable["F0"] = "F0"
_INTERNAL_RegisterNameMappingsTable["F1"] = "F1"
_INTERNAL_RegisterNameMappingsTable["F2"] = "F2"
_INTERNAL_RegisterNameMappingsTable["F3"] = "F3"
_INTERNAL_RegisterNameMappingsTable["F4"] = "F4"
_INTERNAL_RegisterNameMappingsTable["F5"] = "F5"
_INTERNAL_RegisterNameMappingsTable["F6"] = "F6"
_INTERNAL_RegisterNameMappingsTable["F7"] = "F7"
_INTERNAL_RegisterNameMappingsTable["F8"] = "F8"
_INTERNAL_RegisterNameMappingsTable["F9"] = "F9"
_INTERNAL_RegisterNameMappingsTable["F10"] = "F10"
_INTERNAL_RegisterNameMappingsTable["F11"] = "F11"
_INTERNAL_RegisterNameMappingsTable["F12"] = "F12"
_INTERNAL_RegisterNameMappingsTable["F13"] = "F13"
_INTERNAL_RegisterNameMappingsTable["F14"] = "F14"
_INTERNAL_RegisterNameMappingsTable["F15"] = "F15"
_INTERNAL_RegisterNameMappingsTable["F16"] = "F16"
_INTERNAL_RegisterNameMappingsTable["F17"] = "F17"
_INTERNAL_RegisterNameMappingsTable["F18"] = "F18"
_INTERNAL_RegisterNameMappingsTable["F19"] = "F19"
_INTERNAL_RegisterNameMappingsTable["F20"] = "F20"
_INTERNAL_RegisterNameMappingsTable["F21"] = "F21"
_INTERNAL_RegisterNameMappingsTable["F22"] = "F22"
_INTERNAL_RegisterNameMappingsTable["F23"] = "F23"
_INTERNAL_RegisterNameMappingsTable["F24"] = "F24"
_INTERNAL_RegisterNameMappingsTable["F25"] = "F25"
_INTERNAL_RegisterNameMappingsTable["F26"] = "F26"
_INTERNAL_RegisterNameMappingsTable["F27"] = "F27"
_INTERNAL_RegisterNameMappingsTable["F28"] = "F28"
_INTERNAL_RegisterNameMappingsTable["F29"] = "F29"
_INTERNAL_RegisterNameMappingsTable["F30"] = "F30"
_INTERNAL_RegisterNameMappingsTable["F31"] = "F31"
_INTERNAL_RegisterNameMappingsTable["PC"] = "PC"
_INTERNAL_RegisterNameMappingsTable["LR"] = "LR"


_INTERNAL_registers_get_register_table = {}
_INTERNAL_registers_get_register_table[NumberTypes.U8] = RegistersAPI.getU8FromRegister
_INTERNAL_registers_get_register_table[NumberTypes.U16] = RegistersAPI.getU16FromRegister
_INTERNAL_registers_get_register_table[NumberTypes.U32] = RegistersAPI.getU32FromRegister
_INTERNAL_registers_get_register_table[NumberTypes.U64] = RegistersAPI.getU64FromRegister
_INTERNAL_registers_get_register_table[NumberTypes.S8] = RegistersAPI.getS8FromRegister
_INTERNAL_registers_get_register_table[NumberTypes.S16] = RegistersAPI.getS16FromRegister
_INTERNAL_registers_get_register_table[NumberTypes.S32] = RegistersAPI.getS32FromRegister
_INTERNAL_registers_get_register_table[NumberTypes.S64] = RegistersAPI.getS64FromRegister
_INTERNAL_registers_get_register_table[NumberTypes.FLOAT] = RegistersAPI.getFloatFromRegister
_INTERNAL_registers_get_register_table[NumberTypes.DOUBLE] = RegistersAPI.getDoubleFromRegister


def getRegister(registerString, returnTypeString, optionalOffset = None):
	if optionalOffset is None:
		optionalOffset = 0
	elif optionalOffset < 0 or optionalOffset >= 16:
		raise("Error: Offset passed into getRegister() was outside the maximum allowable range of 0-15!")
	
	registerString = registerString.upper()
	
	registerEnum = _INTERNAL_RegisterNameMappingsTable.get(registerString, None)
	
	if registerEnum is None:
		raise("Error: Invalid register name passed into getRegister() function. Valid registers are R0-R31, F0-F31, PC and LR")
	
	returnTypeEnum = ParseType(returnTypeString)
	
	if returnTypeEnum == NumberTypes.UNKNOWN:
		raise("Error: Invalid typename passed into registers:getRegister(). Valid values include U8, U16, U32, U64, S8, S16, S32, S64, FLOAT, DOUBLE, UnsignedByte, SignedByte, UnsignedInt, SignedInt and SignedLongLong")
        
	get_register_function_call = _INTERNAL_registers_get_register_table.get(returnTypeEnum, None)
	
	if get_register_function_call is None:
		raise("Error: An unknown implementation error occured in registers.getRegister()")
	
	return get_register_function_call(registerString, optionalOffset)


def getRegisterAsUnsignedByteArray(registerString, arraySize, optionalOffset = None):
	if optionalOffset is None:
		optionalOffset = 0
	elif optionalOffset < 0 or optionalOffset >= 16:
		raise("Error: Offset passed into getRegisterAsUnsignedByteArray() was outside the maximum allowable range of 0-15!")
	
	registerString = registerString.upper()
	
	registerEnum = _INTERNAL_RegisterNameMappingsTable.get(registerString, None)
	
	if registerEnum is None:
		raise("Error: Invalid register name passed into getRegisterAsUnsignedByteArray(). Valid registers are R0-R31, F0-F31, PC and LR")

	if arraySize <= 0 or arraySize > 16:
		raise("Error: Array size was outside the maximum allowable range of 1-16!")
	
	return RegistersAPI.getUnsignedBytesFromRegister(registerString, arraySize, optionalOffset)


def getRegisterAsSignedByteArray(registerString, arraySize, optionalOffset = None):
	if optionalOffset is None:
		optionalOffset = 0
	elif optionalOffset < 0 or optionalOffset >= 16:
		raise("Error: Offset passed into getRegisterAsSignedByteArray() was outside the maximum allowable range of 0-15!")
	
	registerString = registerString.upper()
	
	registerEnum = _INTERNAL_RegisterNameMappingsTable.get(registerString, None)
	
	if registerEnum is None:
		raise("Error: Invalid register name passed into getRegisterAsSignedByteArray(). Valid registers are R0-R31, F0-F31, PC and LR")
	
	if arraySize <= 0 or arraySize > 16:
		raise("Error: Array size was outside the maximum allowable range of 1-16!")
	
	return RegistersAPI.getSignedBytesFromRegister(registerString, arraySize, optionalOffset)
    
    
_INTERNAL_registers_set_register_table = {}
_INTERNAL_registers_set_register_table[NumberTypes.U8] = RegistersAPI.writeU8ToRegister
_INTERNAL_registers_set_register_table[NumberTypes.U16] = RegistersAPI.writeU16ToRegister
_INTERNAL_registers_set_register_table[NumberTypes.U32] = RegistersAPI.writeU32ToRegister
_INTERNAL_registers_set_register_table[NumberTypes.U64] = RegistersAPI.writeU64ToRegister
_INTERNAL_registers_set_register_table[NumberTypes.S8] = RegistersAPI.writeS8ToRegister
_INTERNAL_registers_set_register_table[NumberTypes.S16] = RegistersAPI.writeS16ToRegister
_INTERNAL_registers_set_register_table[NumberTypes.S32] = RegistersAPI.writeS32ToRegister
_INTERNAL_registers_set_register_table[NumberTypes.S64] = RegistersAPI.writeS64ToRegister
_INTERNAL_registers_set_register_table[NumberTypes.FLOAT] = RegistersAPI.writeFloatToRegister
_INTERNAL_registers_set_register_table[NumberTypes.DOUBLE] = RegistersAPI.writeDoubleToRegister


def setRegister(registerString, writeType, writeValue, optionalOffset = None):
	if optionalOffset is None:
		optionalOffset = 0
	elif optionalOffset < 0 or optionalOffset >= 16:
		raise("Error: Offset passed into setRegister() was outside the maximum allowable range of 0-15!")
	
	registerString = registerString.upper()
	
	registerEnum = _INTERNAL_RegisterNameMappingsTable.get(registerString, None)
	
	if registerEnum is None:
		raise("Error: Invalid register name passed into setRegister() function. Valid registers are R0-R31, F0-F31, PC and LR")
	
	returnTypeEnum = ParseType(writeType)
	
	if returnTypeEnum == NumberTypes.UNKNOWN:
		raise("Error: Invalid typename passed into registers.setRegister(). Valid values include U8, U16, U32, U64, S8, S16, S32, S64, FLOAT, DOUBLE, UnsignedByte, SignedByte, UnsignedInt, SignedInt and SignedLongLong")
	
	set_register_function_call = _INTERNAL_registers_set_register_table.get(returnTypeEnum, None)
	
	if set_register_function_call is None:
		raise("Error: An unknown implementation error occured in registers:setRegister()")
	
	set_register_function_call(registerString, writeValue, optionalOffset)
    


def setRegisterFromByteArray(registerString, indexToByteTable, optionalOffset = None):
    if optionalOffset is None:
        optionalOffset = 0
    elif optionalOffset < 0 or optionalOffset >= 16:
        raise("Error: Offset passed into setRegisterFromByteArray() was outside the maximum allowable range of 0-15!")
	
    registerString = registerString.upper()
	
    registerEnum = _INTERNAL_RegisterNameMappingsTable.get(registerString, None)
	
    if registerEnum is None:
        raise("Error: Invalid register name passed into setRegisterFromByteArray() function. Valud registers are R0-R31, F0-F31, PC and LR")
	
    if indexToByteTable is None:
        raise("Error: In setRegisterFromByteArray(), the indexToByteTable was None!")
	
    RegistersAPI.writeBytesToRegister(registerString, indexToByteTable, optionalOffset)

