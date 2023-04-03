from type_resolver import NumberTypes
from type_resolver import ParseType
import dolphin
dolphin.importModule("MemoryAPI", "1.0")
import MemoryAPI

_INTERNAL_memory_read_from_table = {}
_INTERNAL_memory_read_from_table[NumberTypes.U8] = MemoryAPI.read_u8
_INTERNAL_memory_read_from_table[NumberTypes.S8] = MemoryAPI.read_s8
_INTERNAL_memory_read_from_table[NumberTypes.U16] = MemoryAPI.read_u16
_INTERNAL_memory_read_from_table[NumberTypes.S16] = MemoryAPI.read_s16
_INTERNAL_memory_read_from_table[NumberTypes.U32] = MemoryAPI.read_u32
_INTERNAL_memory_read_from_table[NumberTypes.S32] = MemoryAPI.read_s32
_INTERNAL_memory_read_from_table[NumberTypes.FLOAT] = MemoryAPI.read_float
_INTERNAL_memory_read_from_table[NumberTypes.U64] = MemoryAPI.read_u64
_INTERNAL_memory_read_from_table[NumberTypes.S64] = MemoryAPI.read_s64
_INTERNAL_memory_read_from_table[NumberTypes.DOUBLE] = MemoryAPI.read_double

def readFrom(address, typeString):
    typeEnum = ParseType(typeString)
	
    if typeEnum == NumberTypes.UNKNOWN:
        raise("Error: Unknown type string passed into memory.readFrom(address, typeString) function. Valid type names are: u8, unsigned_byte, unsigned_8, s8, signed_byte, signed_8, u16, unsigned_16, s16, signed_16, u32, unsigned_int, unsigned_32, s32, signed_int, signed_32, float, u64, unsigned_long_long, unsigned_64, s64, signed_long_long, signed_64, and double. Note that underscores in the typename are interchangable with spaces, dashes and the empty string")
	
    return _INTERNAL_memory_read_from_table[typeEnum](address)
		
def readUnsignedBytes(address, numBytes):
    return MemoryAPI.read_unsigned_bytes(address, numBytes)

def readSignedBytes(address, numBytes):
    return MemoryAPI.read_signed_bytes(address, numBytes)

def readFixedLengthString(address, numBytes):
    return MemoryAPI.read_fixed_length_string(address, numBytes)

def readNullTerminatedString(address):
    return MemoryAPI.read_null_terminated_string(address)

_INTERNAL_memory_write_to_table = {}
_INTERNAL_memory_write_to_table[NumberTypes.U8] = MemoryAPI.write_u8
_INTERNAL_memory_write_to_table[NumberTypes.S8] = MemoryAPI.write_s8
_INTERNAL_memory_write_to_table[NumberTypes.U16] = MemoryAPI.write_u16
_INTERNAL_memory_write_to_table[NumberTypes.S16] = MemoryAPI.write_s16
_INTERNAL_memory_write_to_table[NumberTypes.U32] = MemoryAPI.write_u32
_INTERNAL_memory_write_to_table[NumberTypes.S32] = MemoryAPI.write_s32
_INTERNAL_memory_write_to_table[NumberTypes.FLOAT] = MemoryAPI.write_float
_INTERNAL_memory_write_to_table[NumberTypes.U64] = MemoryAPI.write_u64
_INTERNAL_memory_write_to_table[NumberTypes.S64] = MemoryAPI.write_s64
_INTERNAL_memory_write_to_table[NumberTypes.DOUBLE] = MemoryAPI.write_double

def writeTo(address, typeString, value):
    typeEnum = ParseType(typeString)
	
    if typeEnum == NumberTypes.UNKNOWN:
        raise("Error: Unknown type string passed into memory.writeTo(address, typeString, value) function. Valid type names are: u8, unsigned_byte, unsigned_8, s8, signed_byte, signed_8, u16, unsigned_16, s16, signed_16, u32, unsigned_int, unsigned_32, s32, signed_int, signed_32, float, u64, unsigned_long_long, unsigned_64, s64, signed_long_long, signed_64, and double. Note that underscores in the typename are interchangable with spaces, dashes and the empty string")
	
    _INTERNAL_memory_write_to_table[typeEnum](address, value)

def writeBytes(addressToValueMap):
    return MemoryAPI.write_bytes(addressToValueMap)

def writeString(address, stringToWrite):
    return MemoryAPI.write_string(address, stringToWrite)
