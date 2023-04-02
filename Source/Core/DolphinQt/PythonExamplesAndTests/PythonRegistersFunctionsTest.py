import registers

registers.setRegister("R0", "U8", 140, 0)
registers.setRegisterFromByteArray("R0", {0: -2}, 1)

print(str(registers.getRegister("R0", "U8", 0)))
print(str(registers.getRegisterAsUnsignedByteArray("R0", 2, 0)))
print(str(registers.getRegisterAsSignedByteArray("R0", 2, 0)))

