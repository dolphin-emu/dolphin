from enum import Enum

class NumberTypes(Enum):
    U8 = 1
    U16 = 2
    U32 = 3
    U64 = 4
    S8 = 5
    S16 = 6
    S32 = 7
    S64 = 8
    FLOAT = 9
    DOUBLE = 10
    UNKNOWN = 11


internalResolutionTable = {}

internalResolutionTable["U8"] = NumberTypes.U8
internalResolutionTable["UNSIGNEDBYTE"] = NumberTypes.U8
internalResolutionTable["UNSIGNED8"] = NumberTypes.U8

internalResolutionTable["S8"] = NumberTypes.S8
internalResolutionTable["SIGNEDBYTE"] = NumberTypes.S8
internalResolutionTable["SIGNED8"] = NumberTypes.S8

internalResolutionTable["U16"] = NumberTypes.U16
internalResolutionTable["UNSIGNED16"] = NumberTypes.U16

internalResolutionTable["S16"] = NumberTypes.S16
internalResolutionTable["SIGNED16"] = NumberTypes.S16

internalResolutionTable["U32"] = NumberTypes.U32
internalResolutionTable["UNSIGNED32"] = NumberTypes.U32
internalResolutionTable["UNSIGNEDINT"] = NumberTypes.U32

internalResolutionTable["S32"] = NumberTypes.S32
internalResolutionTable["SIGNED32"] = NumberTypes.S32
internalResolutionTable["SIGNEDINT"] = NumberTypes.S32

internalResolutionTable["FLOAT"] = NumberTypes.FLOAT

internalResolutionTable["U64"] = NumberTypes.U64
internalResolutionTable["UNSIGNED64"] = NumberTypes.U64
internalResolutionTable["UNSIGNEDLONGLONG"] = NumberTypes.U64

internalResolutionTable["S64"] = NumberTypes.S64
internalResolutionTable["SIGNED64"] = NumberTypes.S64
internalResolutionTable["SIGNEDLONGLONG"] = NumberTypes.S64

internalResolutionTable["DOUBLE"] = NumberTypes.DOUBLE

def ParseType(typeString):
    if typeString is None or len(typeString) == 0:
        return NumberTypes.UNKNOWN
	
    typeString = typeString.upper()
    typeString = typeString.replace(" ", "")
    typeString = typeString.replace("_", "")
    typeString = typeString.replace("-", "")
    
    return internalResolutionTable.get(typeString, NumberTypes.UNKNOWN)

