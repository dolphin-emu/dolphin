function MyLuaFunction()
	emu.frameAdvance()
	emu.frameAdvance()

	file = io.open("output.txt", "w")
	io.output(file)
	io.write("Value was: ")
	io.write(tostring(ram.read_unsigned_8(84300)))
	io.flush()
	io.close()
end

MyLuaFunction()

