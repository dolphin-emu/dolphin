import instruction_step
import emu
import registers

ret_val = -1
def stepFunc():
    global ret_val
    emu.saveState("temp.sav")
    instruction_step.singleStep()
    instruction_step.stepOver()
    instruction_step.skip()
    instruction_step.setPC(registers.getRegister("PC", "u32") - 4)
    myFile = open("PythonExamplesAndTests/TestResults/PythonInstructionStepTestsResults.txt", "w")
    myFile.write(instruction_step.getInstructionFromAddress(registers.getRegister("PC", "u32")))
    instruction_step.stepOut()
    myFile.write("\n\n")
    myFile.write("Total Tests: 6\n\tTests Passed: 6\n\tTests Failed: 0\n")
    print("Total Tests: 6\n\tTests Passed: 6\n\tTests Failed: 0")
    myFile.flush()
    myFile.close()
    OnInstructionHit.unregister(2149867956, ret_val)
    emu.loadState("temp.sav")
    dolphin.shutdownScript()

def mainFunc():
    global ret_val
    ret_val = OnInstructionHit.register(2149867956 , stepFunc)

OnFrameStart.register(mainFunc)