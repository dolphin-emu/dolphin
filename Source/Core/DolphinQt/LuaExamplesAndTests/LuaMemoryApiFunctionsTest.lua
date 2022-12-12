function printAddressValue(address, value)
	io.write("Value of address " .. tostring(address) .. " now is " .. tostring(value) .. "\n")
end

function printAddressAsTypes(address, typeStringTable) 
	table.sort(typeStringTable)
	for k, v in pairs(typeStringTable) do
		memory:writeTo(k, address, v)
		io.write("After writing " .. tostring(v) .. " to address " .. tostring(address) .. ", value as a " .. k  .. " is: " .. tostring(memory:readFrom(k, address)) .. "\n")	
	end
end

function printMemoryTable(myTable)
	for k, v in pairs(myTable) do
		io.write("Address: " .. tostring(k) .. ", value: " .. tostring(v) .. "\n")
	end
	io.write("\n")
end

function MemoryFunctionsTest()
	address = 0X80000123
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
	memory:writeTo("u64", address, 0)
	
	typeTable = {}
	typeTable["u8"] = byteToWrite
	typeTable["unsigned_byte"] = byteToWrite
	typeTable["UNSIGNED 8"] = byteToWrite

	typeTable["u16"] = shortToWrite
	typeTable["Unsigned_16"] = shortToWrite

	typeTable["u32"] = intToWrite
	typeTable["UNSIGNED_INT"] = intToWrite
	typeTable["unsigned_32"] = intToWrite

	typeTable["u64"] = longLongToWrite
	typeTable["Unsigned LONG_LONG"] = longLongToWrite
	typeTable["UNSIGNED_64"] = longLongToWrite


	typeTable["s8"] = byteToWrite
	typeTable["signed ByTE"] = byteToWrite
	typeTable["SIGNED_8"] = byteToWrite


	typeTable["S16"] = shortToWrite
	typeTable["SIGNED 16"] = shortToWrite


	typeTable["s32"] = intToWrite
	typeTable["signed int"] = intToWrite
	typeTable["signed 32"] = intToWrite


	typeTable["s64"] = longLongToWrite
	typeTable["Signed Long Long"] = longLongToWrite
	typeTable["signed_64"] = longLongToWrite


	typeTable["float"] = floatToWrite

	typeTable["DoUBle"] = doubleToWrite
	
	printAddressAsTypes(address, typeTable)

	io.write("\n\nstring to write out is: " .. stringToWrite .. "\n")
	io.write("Testing out writing string as memory:write_string()\n")
	memory:write_string(address, stringToWrite)
	printAddressValue(address, memory:read_null_terminated_string(address))
	memory:writeTo("u64", address, 0)
	io.write("Testing out reading only 6 chars of string:\n")
	memory:write_string(address, stringToWrite)
	printAddressValue(address, memory:read_fixed_length_string(address, 6))
	memory:writeTo("u64", address, 0)
	io.write("\n\n")

	byteTable = {}
	byteTable[address] = 255
	byteTable[address + 1] = 254
	byteTable[address + 2] = 100
	byteTable[address + 3] = -127
	byteTable[address + 4] = -128
	io.write("Initial byte table that is written:\n")
	printMemoryTable(byteTable)
	memory:write_bytes(byteTable)
	io.write("Byte table read as 5 unsigned bytes:\n")
	printMemoryTable(memory:read_unsigned_bytes(address, 5))
	io.write("Byte table read as 5 signed bytes:\n")
	printMemoryTable(memory:read_signed_bytes(address, 5))

	io.flush()
	io.close()
end

MemoryFunctionsTest()




