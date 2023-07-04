import instruction_step
import registers
import memory

PatCryptDecryptAddress = 2149867956
PatCryptEncryptAddress = 2149867960

def PatCryptDecryptCb():
    #print("Decrypt CB")
    data_ptr = registers.getRegister("R3", "u32", 0)
    #print ("At line 11")
    size_ptr = registers.getRegister("R4", "u32", 0)
    #print("At line 13")
    #print(type(size_ptr))
    size = memory.readFrom(PatCryptEncryptAddress, "u16")
    #print("At line 15")

    #print(f"Decrypt [0x{data_ptr:08X}; 0x{size:04X}]")
    #print("At line 18")

def PatCryptEncryptCb():
    #print("Encrypt CB")
    data_ptr = registers.getRegister("R3", "u32", 0)
    #print("At line 23")
    size_ptr = registers.getRegister("R4", "u32", 0)
    #print("At line 25")
    size = memory.readFrom(PatCryptDecryptAddress, "u16")
    #print("At linr 27")

    #print(f"Encrypt [0x{data_ptr:08X}; 0x{size:04X}]")

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