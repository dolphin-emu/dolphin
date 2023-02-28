require ("type_resolver")
dolphin:importModule("MemoryAPI", "1.0.0")

MemoryClass = {}

function MemoryClass:new(obj)
	obj = obj or {}
	self.__index = self
	return setmetatable(obj, self)
end

memory = MemoryClass:new(nil)

memory_read_from_table = {
[NumberTypes.U8] = function (address) return MemoryAPI:read_u8(address) end,
[NumberTypes.S8] = function (address) return MemoryAPI:read_s8(address) end,
[NumberTypes.U16] = function (address) return MemoryAPI:read_u16(address) end,
[NumberTypes.S16] = function (address) return MemoryAPI:read_s16(address) end,
[NumberTypes.U32] = function (address) return MemoryAPI:read_u32(address) end,
[NumberTypes.S32] = function (address) return MemoryAPI:read_s32(address) end,
[NumberTypes.FLOAT] = function (address) return MemoryAPI:read_float(address) end,
[NumberTypes.U64] = function (address) return MemoryAPI:read_u64(address) end,
[NumberTypes.S64] = function (address) return MemoryAPI:read_s64(address) end,
[NumberTypes.DOUBLE] = function (address) return MemoryAPI:read_double(address) end
}

function MemoryClass:readFrom(address, typeString)
	typeEnum = ParseType(typeString)
	
	if typeEnum == NumberTypes.UNKNOWN then
		error("Error: Unknown type string passed into memory:readFrom(address, typeString) function. Valid type names are: u8, unsigned_byte, unsigned_8, s8, signed_byte, signed_8, u16, unsigned_16, s16, signed_16, u32, unsigned_int, unsigned_32, s32, signed_int, signed_32, float, u64, unsigned_long_long, unsigned_64, s64, signed_long_long, signed_64, and double. Note that underscores in the typename are interchangable with spaces, dashes and the empty string")
	end
	
	return memory_read_from_table[typeEnum](address)
		
end 

function MemoryClass:readUnsignedBytes(address, numBytes)
	return MemoryAPI:read_unsigned_bytes(address, numBytes)
end

function MemoryClass:readSignedBytes(address, numBytes)
	return MemoryAPI:read_signed_bytes(address, numBytes)
end

function MemoryClass:readFixedLengthString(address, numBytes)
	return MemoryAPI:read_fixed_length_string(address, numBytes)
end

function MemoryClass:readNullTerminatedString(address)
	return MemoryAPI:read_null_terminated_string(address)
end

memory_write_to_table = {
[NumberTypes.U8] = function (address, value) return MemoryAPI:write_u8(address, value) end,
[NumberTypes.S8] = function (address, value) return MemoryAPI:write_s8(address, value) end,
[NumberTypes.U16] = function (address, value) return MemoryAPI:write_u16(address, value) end,
[NumberTypes.S16] = function (address, value) return MemoryAPI:write_s16(address, value) end,
[NumberTypes.U32] = function (address,  value) return MemoryAPI:write_u32(address, value) end,
[NumberTypes.S32] = function (address, value) return MemoryAPI:write_s32(address, value) end,
[NumberTypes.FLOAT] = function (address, value) return MemoryAPI:write_float(address, value) end,
[NumberTypes.U64] = function (address, value) return MemoryAPI:write_u64(address, value) end,
[NumberTypes.S64] = function (address, value) return MemoryAPI:write_s64(address, value) end,
[NumberTypes.DOUBLE] = function (address, value) return MemoryAPI:write_double(address, value) end
}

function MemoryClass:writeTo(address, typeString, value)
	typeEnum = ParseType(typeString)
	
	if typeEnum == NumberTypes.UNKNOWN then
		error("Error: Unknown type string passed into memory:writeTo(address, typeString, value) function. Valid type names are: u8, unsigned_byte, unsigned_8, s8, signed_byte, signed_8, u16, unsigned_16, s16, signed_16, u32, unsigned_int, unsigned_32, s32, signed_int, signed_32, float, u64, unsigned_long_long, unsigned_64, s64, signed_long_long, signed_64, and double. Note that underscores in the typename are interchangable with spaces, dashes and the empty string")
	end
	
	memory_write_to_table[typeEnum](address, value)
end

function MemoryClass:writeBytes(addressToValueMap)
	return MemoryAPI:write_bytes(addressToValueMap)
end

function MemoryClass:writeString(address, stringToWrite)
	return MemoryAPI:write_string(address, stringToWrite)
end