dolphin:importModule("BitAPI", "1.0.0")

BitClass = {}

function BitClass:new(obj)
	obj = obj or {}
	self.__index = self
	return setmetatable(obj, self)
end

bit = BitClass:new(nil)

function BitClass:bitwise_and(first_val, second_val)
	return BitAPI:bitwise_and(first_val, second_val)
end

function BitClass:bitwise_or(first_val, second_val)
	return BitAPI:bitwise_or(first_val, second_val)
end

function BitClass:bitwise_not(first_val)
	return BitAPI:bitwise_not(first_val)
end

function BitClass:bitwise_xor(first_val, second_val)
	return BitAPI:bitwise_xor(first_val,  second_val)
end

function BitClass:logical_and(first_val, second_val)
	return BitAPI:logical_and(first_val,  second_val)
end

function BitClass:logical_or(first_val, second_val)
	return BitAPI:logical_or(first_val, second_val)
end

function BitClass:logical_xor(first_val, second_val)
	return BitAPI:logical_xor(first_val,  second_val)
end

function BitClass:logical_not(first_val)
	return BitAPI:logical_not(first_val)
end

function BitClass:bit_shift_left(first_val, second_val)
	return BitAPI:bit_shift_left(first_val, second_val)
end

function BitClass:bit_shift_right(first_val, second_val)
	return BitAPI:bit_shift_right(first_val, second_val)
end