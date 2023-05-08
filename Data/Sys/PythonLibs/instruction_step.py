import dolphin
dolphin.importModule("InstructionStepAPI", "1.0")
import InstructionStepAPI

def singleStep():
    InstructionStepAPI.singleStep()

def stepOver():
    InstructionStepAPI.stepOver()

def stepOut():
    InstructionStepAPI.stepOut()

def skip():
    InstructionStepAPI.skip()

def setPC(newPCValue):
    InstructionStepAPI.setPC(newPCValue)

def getInstructionFromAddress(address):
    return InstructionStepAPI.getInstructionFromAddress(address)

