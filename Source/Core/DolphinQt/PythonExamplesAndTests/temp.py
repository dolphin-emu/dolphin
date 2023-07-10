"""import instruction_step
import registers
import memory

PatCryptDecryptAddress =2149867956
PatCryptEncryptAddress = 2149867960

def PatCryptDecryptCb():
    try:
        print("Decrypt CB")
        data_ptr = registers.getRegister("R3", "u32", 0)
        size_ptr = registers.getRegister("R4", "u32", 0)
        size = memory.readFrom(size_ptr, "u16")
        print(f"Decrypt [0x{data_ptr:08X}; 0x{size:04X}]")
    except:
        print("Exception occured!")


def PatCryptEncryptCb():
    print("Encrypt CB")
    data_ptr = registers.getRegister("R3", "u32", 0)
    size_ptr = registers.getRegister("R4", "u32", 0)
    size = memory.readFrom(size_ptr, "u16")

    print(f"Encrypt [0x{data_ptr:08X}; 0x{size:04X}]")

PatCryptDecryptKey = -1
PatCryptEncryptKey = -1

def mainFunc():
    print("Main Func")
    global PatCryptDecryptKey
    global PatCryptEncryptKey
    
print("Python file")
# OnFrameStart.registerWithAutoDeregistration(mainFunc)

PatCryptDecryptKey = OnInstructionHit.register(PatCryptDecryptAddress, PatCryptDecryptCb)
PatCryptEncryptKey = OnInstructionHit.register(PatCryptEncryptAddress, PatCryptEncryptCb)
"""

# This is from the Source/Core/Core/Scripting/Documentation/PythonRawAPIDocumentation.txt file
dolphin.importModule("EmuAPI", "1.0")
dolphin.importModule("MemoryAPI", "1.0")

def myCheckMemoryFunction():
	if MemoryAPI.read_u8(0X80000008) != 42:
		print("Byte at 0X80000008 was NOT 42!")

OnFrameStart.register(myCheckMemoryFunction)