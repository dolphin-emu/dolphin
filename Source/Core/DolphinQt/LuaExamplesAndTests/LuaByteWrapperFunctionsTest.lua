function printByteWrapperAsTypes(typeStringTable)
	table.sort(typeStringTable)
	for k, v in pairs(typeStringTable) do
		tempByteWrapper = ByteWrapper(v)
		tempByteWrapper = tempByteWrapper:with_type(k)
		io.write("After creating a ByteWrapper with value " .. tostring(v) .. ", value as a " .. k .. " is: " .. tostring(tempByteWrapper:get_value()) .. "\n")
		io.flush()
	end
end

function ByteWrapperFunctionsTest()
	address = 0X80000123
	byteToWrite = 230
	shortToWrite = 60000
	intToWrite = 4000000000
	longLongToWrite = 10123456789123456789
	floatToWrite = 5e+20
	doubleToWrite = 6.022e+23

	emu.frameAdvance()
	emu.frameAdvance()
	
	file = io.open("ByteWrapperTestsOutput.txt", "w")

	io.output(file)

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
	
	printByteWrapperAsTypes(typeTable)

	io.flush()
	io.close()

end

ByteWrapperFunctionsTest()