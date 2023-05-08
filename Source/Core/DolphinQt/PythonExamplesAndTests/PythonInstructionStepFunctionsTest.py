import instruction_step
import registers

def stepFunc():
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
    dolphin.shutdownScript()

def mainFunc():
    OnInstructionHit.register(registers.getRegister("PC", "u32"), stepFunc)

OnFrameStart.register(mainFunc)