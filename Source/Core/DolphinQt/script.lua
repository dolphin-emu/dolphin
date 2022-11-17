function printAddressValue(address, value)
	io.write("Value of address " .. tostring(address) .. " now is " .. tostring(value) .. "\n")
end

function MyLuaFunction()
	address = 84300
	byteToWrite = 230
	shortToWrite = 60000
	intToWrite = 4000000000
	longLongToWrite = 10123456789123456789.
	floatToWrite = 5e+20
	doubleToWrite = 6.022e+23
	stringToWrite = "this_is_a_string"

	emu.frameAdvance()
	emu.frameAdvance()

	file = io.open("output.txt", "w")
	io.output(file)
	io.write("Zeroing out address " .. tostring(address) .. " to start out with...\n")
	ram.write_unsigned_64(address, 0)
	
	io.write("Value of address " .. tostring(address) .. " after getting zeroed out: " .. tostring(ram.read_unsigned_64(address, 0)) .. "\n")

	io.write("Byte value to write is: " .. tostring(byteToWrite) .. "\n")
	io.write("Testing out writing byte as ram.write_unsigned_8()\n")
	ram.write_unsigned_8(address, byteToWrite)
	printAddressValue(address, ram.read_unsigned_8(address))

	ram.write_unsigned_64(address, 0)
	io.write("Testing out writing byte as ram.write_signed_8()\n")
	ram.write_signed_8(address, byteToWrite)
	printAddressValue(address, ram.read_signed_8(address))
	ram.write_unsigned_64(address, 0)
	io.write("Testing out writing byte as ram.write_unsigned_byte()\n")
	ram.write_unsigned_byte(address, byteToWrite)
	printAddressValue(address, ram.read_unsigned_byte(address))
	ram.write_unsigned_64(address, 0)
	io.write("Testing out writing byte as ram.write_signed_byte()\n")
	ram.write_signed_byte(address, byteToWrite)
	printAddressValue(address, ram.read_signed_byte(address))
	ram.write_unsigned_64(address, 0)
	io.write("\n\n\n")

	io.write("Short value to write is: " .. tostring(shortToWrite) .. "\n")
	io.write("Testing out writing short as ram.write_unsigned_16()\n")
	ram.write_unsigned_16(address, shortToWrite)
	printAddressValue(address, ram.read_unsigned_16(address))
	ram.write_unsigned_64(address, 0)
	io.write("Testing out writing short as ram.write_signed_16()\n")
	ram.write_signed_16(address, shortToWrite)
	printAddressValue(address, ram.read_signed_16(address))
	ram.write_unsigned_64(address, 0)
	io.write("\n\n\n")

	io.write("Int value to write is: " .. tostring(intToWrite) .. "\n")
	io.write("Testing out writing int as ram.write_unsigned_32()\n")
	ram.write_unsigned_32(address, intToWrite)
	printAddressValue(address, ram.read_unsigned_32(address))
	ram.write_unsigned_64(address, 0)
	io.write("Testing out writing int as ram.write_signed_32()\n")
	ram.write_signed_32(address, intToWrite)
	printAddressValue(address, ram.read_signed_32(address))
	ram.write_unsigned_64(address, 0)
	io.write("Testing out writing int as ram.write_unsigned_int()\n")
	ram.write_unsigned_int(address, intToWrite)
	printAddressValue(address, ram.read_unsigned_int(address))
	ram.write_unsigned_64(address, 0)
	io.write("Testing out writing int as ram.write_signed_int()\n")
	ram.write_signed_int(address, intToWrite)
	printAddressValue(address, ram.read_signed_int(address))
	ram.write_unsigned_64(address, 0)
	io.write("\n\n\n")

	io.write("Long Long value to write is: " .. tostring(longLongToWrite) .. "\n")
	io.write("Testing out writing long long as ram.write_unsigned_64()\n")
	ram.write_unsigned_64(address, longLongToWrite)
	printAddressValue(address, ram.read_unsigned_64(address))
	ram.write_unsigned_64(address, 0)
	io.write("Testing out writing long long as ram.write_signed_64()\n")
	ram.write_signed_64(address, longLongToWrite)
	printAddressValue(address, ram.read_signed_64(address))
	ram.write_unsigned_64(address, 0)
	io.write("\n\n\n")

	io.write("Float value to write is: " .. tostring(floatToWrite) .. "\n")
	io.write("Testing out writing float as ram.write_float()\n")
	ram.write_float(address, floatToWrite)
	printAddressValue(address, ram.read_float(address))
	ram.write_unsigned_64(address, 0)
	io.write("Double value to write is: " .. tostring(doubleToWrite) .. "\n")
	io.write("Testing out writing double as ram.write_double()\n")
	ram.write_double(address, doubleToWrite)
	printAddressValue(address, ram.read_double(address))
	ram.write_unsigned_64(address, 0)
	io.write("\n\n\n")

	io.write("string to write out is: " .. stringToWrite .. "\n")
	io.write("Testing out writing string as write_fixed_length_string(6)\n")
	ram.write_fixed_length_string(address, stringToWrite, 6)
	printAddressValue(address, ram.read_fixed_length_string(address, 6))
	ram.write_unsigned_64(address, 0)
	io.write("Testing out writing string as write_null_terminated_string()\n")
	ram.write_null_terminated_string(address, stringToWrite)
	printAddressValue(address, ram.read_null_terminated_string(address))
	ram.write_unsigned_64(address, 0)
	io.write("\n\n\n")

	io.flush()
	io.close()
end

MyLuaFunction()

