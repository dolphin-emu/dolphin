
NumberTypes = {U8 = 1, U16 = 2, U32 = 3, U64 = 4, S8 = 5, S16 = 6, S32 = 7, S64 = 8, FLOAT = 9, DOUBLE = 10, UNKNOWN = 11}

internalResolutionTable = {
U8 = "U8",
UNSIGNEDBYTE = "U8",
UNSIGNED8 = "U8",

S8 = "S8",
SIGNEDBYTE = "S8",
SIGNED8 = "S8",

U16 = "U16",
UNSIGNED16 = "U16",

S16 = "S16",
SIGNED16 = "S16",

U32 = "U32",
UNSIGNED32 = "U32",
UNSIGNEDINT = "U32",

S32 = "S32",
SIGNED32 = "S32",
SIGNEDINT = "S32",

FLOAT = "FLOAT",

U64 = "U64",
UNSIGNED64 = "U64",
UNSIGNEDLONGLONG = "U64",

S64 = "S64",
SIGNED64 = "S64",
SIGNEDLONGLONG = "S64",

DOUBLE = "DOUBLE"

}
function ParseType(typeString)
	if typeString == nil or string.len(typeString) == 0 then
		return NumberTypes.UNKNOWN
	end
	
	typeString = string.upper(typeString)
	typeString = typeString:gsub(" ", "")
	typeString = typeString:gsub("_", "")
	typeString = typeString:gsub("-", "")
	
	returnTypeString = internalResolutionTable[typeString]
	
	if returnTypeString ~= nil then
		return NumberTypes[returnTypeString]
	else
		return NumberTypes.UNKNOWN
	end
end