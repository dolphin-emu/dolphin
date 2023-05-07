import filecmp
import random
import os

dolphin.importModule("EmuAPI", "1.0")
dolphin.importModule("MemoryAPI", "1.0")
dolphin.importModule("StatisticsAPI", "1.0")
dolphin.importModule("RegistersAPI", "1.0")
dolphin.importModule("InstructionStepAPI", "1.0")

baseDirectory = "C:/Users/skyle/OneDrive/Desktop/lobsterZeldaDolphin/Source/Core/DolphinQT/DesyncCauseFinderScript/"
frameStartCallbackReference = -1

initialAdvance = 0

# Valid state values for the program
UNKNOWN_STATE = 0
CREATING_POTENTIAL_DESYNCING_SAVE_STATES = 1
TESTING_FOR_DESYNCING_SAVE_STATES = 2
SEARCHING_FOR_DESYNCING_FRAME_IN_ORIGINAL = 3
SEARCHING_FOR_DESYNCING_FRAME_IN_NEW = 4
COUNTING_TOTAL_INSTRUCTIONS = 5
SEARCHING_FOR_DESYNCING_INSTRUCTION_IN_ORIGINAL = 6
SEARCHING_FOR_DESYNCING_INSTRUCTION_IN_NEW = 7
PRINTING_DESYNCING_INSTRUCTION = 8
ERROR_STATE = 9

script_mode = UNKNOWN_STATE

#error codes
SAVE_STATE_NOT_INCLUDED_IN_PROPERTIES = 1
SAVE_STATE_NOT_A_VALID_FILE_IN_PROPERTIES = 2
FRAME_SAVE_STATE_NOT_FOUND = 3

CONTINUE_EXIT_CODE = 42
USER_ABORTED_EXIT_CODE = 0
FINISHED_SCRIPT_EXIT_CODE = 100
INTERNAL_ERROR_CODE = 5

list_of_frames_to_search = []

frameDesyncingSaveStateWasMade = -1
beforeDesyncingFrame = -1
atOrAfterDesyncingFrame = 999999999
totalInstructions = -1
beforeDesyncingInstruction = -1
atOrAfterDesyncingInstruction = 999999999
frameToCheck = -1
instructionToCheck = -1

desyncingFrameNum = -1
desyncingInstructionNum = -1

IS_ORIGINAL = 0
IS_NEW = 1

IS_FRAME = 0
IS_INSTRUCTION = 1

def getMemoryFilename(isOriginal, isFrame, frameOrInstrNumber):
    returnVal = "orig_"
    if isOriginal == IS_NEW:
        returnVal = "new_"
    if isFrame == IS_FRAME:
        returnVal = returnVal + "frame_"
    else:
        returnVal = returnVal + "instruction_"
    returnVal = returnVal + str(frameOrInstrNumber) + ".mem"
    return baseDirectory + returnVal
    
def mapFromStateIntToString(stateValue):
    if stateValue == UNKNOWN_STATE:
        return "UNKNOWN_STATE"
    elif stateValue == CREATING_POTENTIAL_DESYNCING_SAVE_STATES:
        return "CREATING_POTENTIAL_DESYNCING_SAVE_STATES"
    elif stateValue == TESTING_FOR_DESYNCING_SAVE_STATES:
        return "TESTING_FOR_DESYNCING_SAVE_STATES"
    elif stateValue == SEARCHING_FOR_DESYNCING_FRAME_IN_ORIGINAL:
        return "SEARCHING_FOR_DESYNCING_FRAME_IN_ORIGINAL"
    elif stateValue == SEARCHING_FOR_DESYNCING_FRAME_IN_NEW:
        return "SEARCHING_FOR_DESYNCING_FRAME_IN_NEW"
    elif stateValue == SEARCHING_FOR_DESYNCING_INSTRUCTION_IN_ORIGINAL:
        return "SEARCHING_FOR_DESYNCING_INSTRUCTION_IN_ORIGINAL"
    elif stateValue == SEARCHING_FOR_DESYNCING_INSTRUCTION_IN_NEW:
        return "SEARCHING_FOR_DESYNCING_INSTRUCTION_IN_NEW"
    elif stateValue == PRINTING_DESYNCING_INSTRUCTION:
        return "PRINTING_DESYNCING_INSTRUCTION"
    else:
        return "ERROR_STATE"
        
def mapFromStateStringToInt(stateValue):
    if stateValue == "UNKNOWN_STATE":
        return UNKNOWN_STATE
    elif stateValue == "CREATING_POTENTIAL_DESYNCING_SAVE_STATES":
        return CREATING_POTENTIAL_DESYNCING_SAVE_STATES
    elif stateValue == "TESTING_FOR_DESYNCING_SAVE_STATES":
        return TESTING_FOR_DESYNCING_SAVE_STATES
    elif stateValue == "SEARCHING_FOR_DESYNCING_FRAME_IN_ORIGINAL":
        return SEARCHING_FOR_DESYNCING_FRAME_IN_ORIGINAL
    elif stateValue == "SEARCHING_FOR_DESYNCING_FRAME_IN_NEW":
        return SEARCHING_FOR_DESYNCING_FRAME_IN_NEW
    elif stateValue == "SEARCHING_FOR_DESYNCING_INSTRUCTION_IN_ORIGINAL":
        return SEARCHING_FOR_DESYNCING_INSTRUCTION_IN_ORIGINAL
    elif stateValue == "SEARCHING_FOR_DESYNCING_INSTRUCTION_IN_NEW":
        return SEARCHING_FOR_DESYNCING_INSTRUCTION_IN_NEW
    elif stateValue == "PRINTING_DESYNCING_INSTRUCTION":
        return PRINTING_DESYNCING_INSTRUCTION
    else:
        return UNKNOWN_STATE
    

def loadEarlySaveState():
    earlySaveStateName = ""
    propertiesFile = open(baseDirectory + "properties.txt", "r")
    lines = propertiesFile.readlines()
    
    for line in lines:
        pairOfValues = line.strip().split(":", 1)
        if len(pairOfValues) < 2:
            continue
        key = pairOfValues[0].strip().lower()
        value = pairOfValues[1].strip().lower()
        if key == "early_save_state_name" or key == "early_savestate_name":
            earlySaveStateName = value
            break
    if earlySaveStateName == "":
        print("Error: starting save state name was not specified. Properties.txt should include a line \"Early_Save_State_Name:dir1/dir2/saveStateName.sav\"\n")
        dolphin.exitDolphin(SAVE_STATE_NOT_INCLUDED_IN_PROPERTIES)
    elif not os.path.isfile(earlySaveStateName):
        print("Error: starting save state name of " + earlySaveStateName + " did not refer to a valid file!\n")
        dolphin.exitDolphin(SAVE_STATE_NOT_A_VALID_FILE_IN_PROPERTIES)
    EmuAPI.loadState(earlySaveStateName)
    
def getSaveStateNameForFrame(frameNumber):
    return baseDirectory + "F" + str(frameNumber) + ".sav"
    
def loadSaveStateForFrame(frameNumber):
    saveStateName = getSaveStateNameForFrame(frameNumber)
    if not os.path.isfile(saveStateName):
        print("Error: Could not find save state created on frame " + str(frameNumber) + ". Expecting a file named " + saveStateName + "to exist\n")
        dolphin.exitDolphin(FRAME_SAVE_STATE_NOT_FOUND)
    EmuAPI.loadState(saveStateName)

def saveStateForFrame(frameNumber):
    saveStateName = getSaveStateNameForFrame(frameNumber)
    EmuAPI.saveState(saveStateName)

def setupForNewFrameSearch():
    global list_of_frames_to_search
    global script_mode
    loadEarlySaveState()
    continueFile = open(baseDirectory + "continueFile.txt", "w")
    continueFile.close()
    currentFrame = StatisticsAPI.getCurrentFrame()
    movieLength = StatisticsAPI.getMovieLength()
    list_of_frames_to_search = []
    while len(list_of_frames_to_search) < 10:
        next_frame = random.randint(currentFrame + 2, movieLength - 5)
        while next_frame in list_of_frames_to_search:
            next_frame = random.randint(currentFrame + 2, movieLength - 5)
        list_of_frames_to_search.append(next_frame)
    script_mode = CREATING_POTENTIAL_DESYNCING_SAVE_STATES
    
def writeNewFrameSearchToFile():
    continueFile = open(baseDirectory + "continueFile.txt", "w")
    continueFile.write(mapFromStateIntToString(TESTING_FOR_DESYNCING_SAVE_STATES) + "\n")
    for frame in list_of_frames_to_search:
        continueFile.write("TRY " + str(frame) + "\n")
    continueFile.flush()
    continueFile.close()

def parseLowerAndUpperBoundsFromFile(startingLowerBound, startingUpperBound):
    continueFile = open(baseDirectory + "continueFile.txt", "r")
    lines = continueFile.readlines()
    continueFile.close()
    for line in lines:
        splitArrayOnSpace = line.strip().split(" ")
        if not len(splitArrayOnSpace) == 2:
            continue
        if splitArrayOnSpace[0] == ">=":
            tempNum = int(splitArrayOnSpace[1].strip())
            if tempNum > startingLowerBound:
                startingLowerBound = tempNum
        elif splitArrayOnSpace[0] == "<":
            tempNum = int(splitArrayOnSpace[1].strip())
            if tempNum < startingUpperBound:
                startingUpperBound = tempNum
    return (startingLowerBound, startingUpperBound)

def parseModeFromFile():
    global list_of_frames_to_search
    global frameDesyncingSaveStateWasMade
    global beforeDesyncingFrame
    global atOrAfterDesyncingFrame
    global totalInstructions
    global beforeDesyncingInstruction
    global atOrAfterDesyncingInstruction
    global frameToCheck
    global instructionToCheck
    global desyncingFrameNum
    global desyncingInstructionNum
    global script_mode
    
    continueFile = open(baseDirectory + "continueFile.txt", "r")
    lines = continueFile.readlines()
    continueFile.close()
    script_mode = mapFromStateStringToInt(lines[0].strip())
    print("Script mode was: " + str(script_mode))
    if script_mode == UNKNOWN_STATE:
        print("Value of lines is " + str(lines))
        print("Value of lines[0] is: " + lines[0])
        print("Error: Could not parse the contents of continueFile.txt. If you modified the contents of this file, then please delete it and run the script again!")
        dolphin.shutdownScript()
        return
        
    if script_mode >= SEARCHING_FOR_DESYNCING_FRAME_IN_ORIGINAL:
        frameDesyncingSaveStateWasMade = int(lines[1].strip().split(" ")[1].strip())
    if script_mode >= COUNTING_TOTAL_INSTRUCTIONS:
        desyncingFrameNum = int(lines[2].strip().split(" ")[1].strip())
    if script_mode >= SEARCHING_FOR_DESYNCING_INSTRUCTION_IN_ORIGINAL:
        totalInstructions = int(lines[3].strip().split(" ")[1].strip())
        
    if script_mode == TESTING_FOR_DESYNCING_SAVE_STATES:
        list_of_frames_to_search = []
        #debugFile = open(baseDirectory + "debugInfo.txt", "a")
        for line in lines:
            #debugFile.write("Current line: \"" + line + "\"\n")
            splitArrayOnSpace = line.strip().split(" ")
            if not len(splitArrayOnSpace) == 2:
                continue
            if splitArrayOnSpace[0] == "TRY":
                list_of_frames_to_search.append(int(splitArrayOnSpace[1]))
            elif splitArrayOnSpace[0] == "CHECKED":
                list_of_frames_to_search.remove(int(splitArrayOnSpace[1]))
        if len(list_of_frames_to_search) == 0:
            setupForNewFrameSearch()
            return
        else:
            frameToCheck = list_of_frames_to_search[0]
            loadSaveStateForFrame(frameToCheck)
            return
            
    elif script_mode == SEARCHING_FOR_DESYNCING_FRAME_IN_ORIGINAL:
        beforeDesyncingFrame = frameDesyncingSaveStateWasMade
        atOrAfterDesyncingFrame = StatisticsAPI.getMovieLength() - 2
        (beforeDesyncingFrame, atOrAfterDesyncingFrame) = parseLowerAndUpperBoundsFromFile(beforeDesyncingFrame, atOrAfterDesyncingFrame)
        
        if atOrAfterDesyncingFrame - 1 == beforeDesyncingFrame:
            desyncingFrameNum = beforeDesyncingFrame
            script_mode = COUNTING_TOTAL_INSTRUCTIONS
            writeGeneralContinueFile()
            parseModeFromFile()
            return
        if beforeDesyncingFrame == frameDesyncingSaveStateWasMade and atOrAfterDesyncingFrame == StatisticsAPI.getMovieLength() - 2:
            frameToCheck = beforeDesyncingFrame + 1
        else:
            frameToCheck = beforeDesyncingFrame + ((atOrAfterDesyncingFrame - beforeDesyncingFrame) // 2)
        loadEarlySaveState()
        return
        
    elif script_mode == SEARCHING_FOR_DESYNCING_FRAME_IN_NEW:
        beforeDesyncingFrame = frameDesyncingSaveStateWasMade
        atOrAfterDesyncingFrame = StatisticsAPI.getMovieLength() - 2
        (beforeDesyncingFrame, atOrAfterDesyncingFrame) = parseLowerAndUpperBoundsFromFile(beforeDesyncingFrame, atOrAfterDesyncingFrame)
        if atOrAfterDesyncingFrame - 1 == beforeDesyncingFrame:
            desyncingFrameNum = beforeDesyncingFrame
            script_mode = COUNTING_TOTAL_INSTRUCTIONS
            writeGeneralContinueFile()
            parseModeFromFile()
            return
        for line in lines:
            splitArrayOnSpace = line.strip().split(" ")
            if not len(splitArrayOnSpace) == 2:
                continue
            if splitArrayOnSpace[0].strip() == "TRY":
                frameToCheck = int(splitArrayOnSpace[1].strip())
                break
        loadSaveStateForFrame(frameDesyncingSaveStateWasMade)
        return
        
    elif script_mode == COUNTING_TOTAL_INSTRUCTIONS:
        loadEarlySaveState()
        return
        
    elif script_mode == SEARCHING_FOR_DESYNCING_INSTRUCTION_IN_ORIGINAL:
        beforeDesyncingInstruction = 0
        atOrAfterDesyncingInstruction = totalInstructions
        (beforeDesyncingInstruction, atOrAfterDesyncingInstruction) = parseLowerAndUpperBoundsFromFile(beforeDesyncingInstruction, atOrAfterDesyncingInstruction)
        if atOrAfterDesyncingInstruction - 1 == beforeDesyncingInstruction:
            desyncingInstructionNum = beforeDesyncingInstruction
            script_mode = PRINTING_DESYNCING_INSTRUCTION
            writeGeneralContinueFile()
            parseModeFromFile()
            return
        instructionToCheck = beforeDesyncingInstruction + ((atOrAfterDesyncingInstruction - beforeDesyncingInstruction) // 2)
        loadEarlySaveState()
        return
        
    elif script_mode == SEARCHING_FOR_DESYNCING_INSTRUCTION_IN_NEW:
        beforeDesyncingInstruction = 0
        atOrAfterDesyncingInstruction = totalInstructions
        (beforeDesyncingInstruction, atOrAfterDesyncingInstruction) = parseLowerAndUpperBoundsFromFile(beforeDesyncingInstruction, atOrAfterDesyncingInstruction)
        if atOrAfterDesyncingInstruction - 1 == beforeDesyncingInstruction:
            desyncingInstructionNum = beforeDesyncingInstruction
            script_mode = PRINTING_DESYNCING_INSTRUCTION
            writeGeneralContinueFile()
            parseModeFromFile()
            return
        
        for line in lines:
            splitArrayOnSpace = line.strip().split(" ")
            if not len(splitArrayOnSpace) == 2:
                continue
            if splitArrayOnSpace[0].strip() == "TRY":
                instructionToCheck = int(splitArrayOnSpace[1].strip())
                break
        loadSaveStateForFrame(frameDesyncingSaveStateWasMade)
        return
        
    elif script_mode == PRINTING_DESYNCING_INSTRUCTION:
        desyncingInstructionNum = int(lines[4].strip().split(" ")[1].strip())
        loadSaveStateForFrame(frameDesyncingSaveStateWasMade)
        return
        
    else:
        print("Error: Unknown implementation error occurred at end of parseModeFromFile() function...\n")
        dolphin.exitDolphin(ERROR_STATE)
        return

def memoryFilesAreTheSame(mem1, mem2):
    filecmp.clear_cache()
    debugFile = open(baseDirectory + "debugFile.txt", "w")
    if filecmp.cmp(mem1, mem2, shallow=False):
        debugFile.write("Files were the same!\nFile names were:\n" + mem1 + ", and " + mem2 + "\n")
        debugFile.flush()
        debugFile.close()
        return True
    else:
        debugFile.write("Files were different!\nFile names were:\n" + mem1 + ", and " + mem2 + "\n")
        debugFile.flush()
        debugFile.close()
        return False

def writeGeneralContinueFile():
    continueFile = open(baseDirectory + "continueFile.txt", "w")
    continueFile.write(mapFromStateIntToString(script_mode) + "\n")
    if script_mode >= SEARCHING_FOR_DESYNCING_FRAME_IN_ORIGINAL:
        continueFile.write("FRAME_DESYNCING_SAVE_STATE_WAS_MADE: " + str(frameDesyncingSaveStateWasMade) + "\n")
    if script_mode >= COUNTING_TOTAL_INSTRUCTIONS:
        continueFile.write("FRAME_OF_DESYNC_WAS: " + str(desyncingFrameNum) + "\n")
    if script_mode >= SEARCHING_FOR_DESYNCING_INSTRUCTION_IN_ORIGINAL:
        continueFile.write("TOTAL_INSTRUCTIONS_IN_DESYNCING_FRAME: " + str(totalInstructions) + "\n")
    if script_mode >= PRINTING_DESYNCING_INSTRUCTION:
        continueFile.write("DESYNCING_INSTRUCTION: " + str(desyncingInstruction) + "\n")
    continueFile.flush()
    continueFile.close()
  
def countTotalInstructionsFunction():
    global totalInstructions
    global script_mode
    totalInstructions = 0
    currentFrame = StatisticsAPI.getCurrentFrame()
    while StatisticsAPI.getCurrentFrame() == currentFrame:
        InstructionStepAPI.singleStep()
        totalInstructions = totalInstructions + 1
    script_mode = SEARCHING_FOR_DESYNCING_INSTRUCTION_IN_ORIGINAL
    writeGeneralContinueFile()
    continueFile = open(baseDirectory + "continueFile.txt", "a")
    continueFile.write(">= 0\n")
    continueFile.write("< " + str(totalInstructions) + "\n")
    continueFile.flush()
    continueFile.close()
    dolphin.exitDolphin(CONTINUE_EXIT_CODE)

def searchingForDesyncingInstructionInOriginalFunc():
    global script_mode
    currentInstruction = 0
    while currentInstruction < instructionToCheck:
        InstructionStepAPI.singleStep()
        currentInstruction = currentInstruction + 1
    MemoryAPI.writeAllMemoryAsUnsignedBytesToFile(getMemoryFilename(IS_ORIGINAL, IS_INSTRUCTION, currentInstruction))
    script_mode = SEARCHING_FOR_DESYNCING_INSTRUCTION_IN_NEW
    writeGeneralContinueFile()
    continueFile = open(baseDirectory + "continueFile.txt", "a")
    continueFile.write(">= " + str(beforeDesyncingInstruction) + "\n")
    continueFile.write("< " + str(atOrAfterDesyncingInstruction) + "\n")
    continueFile.write("TRY " + str(currentInstruction) + "\n")
    continueFile.flush()
    continueFile.close()
    dolphin.exitDolphin(CONTINUE_EXIT_CODE)

def searchingForDesyncingInstructionInNewFunc():
    global script_mode
    global atOrAfterDesyncingInstruction
    global beforeDesyncingInstruction
    currentInstruction = 0
    while currentInstruction < instructionToCheck:
        InstructionStepAPI.singleStep()
        currentInstruction = currentInstruction + 1
    MemoryAPI.writeAllMemoryAsUnsignedBytesToFile(getMemoryFilename(IS_NEW, IS_INSTRUCTION, currentInstruction))
    script_mode = SEARCHING_FOR_DESYNCING_INSTRUCTION_IN_ORIGINAL
    if memoryFilesAreTheSame(getMemoryFilename(IS_ORIGINAL, IS_INSTRUCTION, currentInstruction), getMemoryFilename(IS_NEW, IS_INSTRUCTION, currentInstruction)): 
    # means desync hasn't happened
        if currentInstruction > beforeDesyncingInstruction:
            beforeDesyncingInstruction = currentInstruction
    else:
    #means desync happened.
        if currentInstruction < atOrAfterDesyncingInstruction:
            atOrAfterDesyncingInstruction = currentInstruction
    os.remove(getMemoryFilename(IS_ORIGINAl, IS_INSTRUCTION, currentInstruction))
    os.remove(getMemoryFilename(IS_NEW, IS_INSTRUCTION, currentInstruction))
    writeGeneralContinueFile()
    continueFile = open(baseDirectory + "continueFile.txt", "a")
    continueFile.write(">= " + str(beforeDesyncingInstruction) + "\n")
    continueFile.write("< " + str(atOrAfterDesyncingInstruction) + "\n")
    continueFile.flush()
    continueFile.close()
    dolphin.exitDolphin(CONTINUE_EXIT_CODE)

def printingDesyncingInstructionFunc():
    currentInstruction = 0
    while currentInstruction < desyncingInstructionNum - 20:
        InstructionStepAPI.singleStep()
        currentInstruction = currentInstruction + 1
    resultFile = open(baseDirectory + "results.txt", "w")
    while currentInstruction < desyncingInstructionNum:
        resultsFile.write(InstructionStepAPI.getInstructionFromAddress(RegistersAPI.getU32FromRegister("PC", 0)) + "\n")
        InstructionStepAPI.singleStep()
        currentInstruction = currentInstruction + 1
    resultsFile.write("DESYNCING INSTRUCTION OCCURS IMMEDIATELY BELOW THIS LINE:\n")
    resultsFile.write("\n" + InstructionStepAPI.getInstructionFromAddress(RegistersAPI.getU32FromRegister("PC", 0)) + "\n\n")
    InstructionStepAPI.singleStep()
    currentInstruction = currentInstruction + 1
    while currentInstruction < desyncingInstructionNum + 20:
        resultsFile.write(InstructionStepAPI.getInstructionFromAddress(RegistersAPI.getU32FromRegister("PC", 0)) + "\n")
        InstructionStepAPI.singleStep()
        currentInstruction = currentInstruction + 1
    resultsFile.flush()
    resultsFile.close()
    dolphin.exitDolphin(FINISHED_SCRIPT_EXIT_CODE)

def mainFunction():
    global initialAdvance
    global script_mode
    global frameDesyncingSaveStateWasMade
    global beforeDesyncingFrame
    global atOrAfterDesyncingFrame
    global totalInstructions
    global beforeDesyncingInstruction
    global atOrAfterDesyncingInstruction
    global frameToCheck
    global instructionToCheck
    global desyncingFrameNum
    global desyncingInstructionNum
    
    if initialAdvance < 3: #We want to let the emulator advance for 3 frames before we start loading a save state (just to get out of booting mode).
        initialAdvance = initialAdvance + 1
        return
    if script_mode == UNKNOWN_STATE: #In this case, we've advanced 3 frames, and are now ready to figure out what mode we're in using the continueFile.txt
        if not os.path.isfile(baseDirectory + "continueFile.txt"):
            setupForNewFrameSearch()
        else:
            continueFile = open(baseDirectory + "continueFile.txt", "r")
            contentsOfContinueFile = continueFile.read().strip()
            continueFile.close()
            if contentsOfContinueFile == "":
                setupForNewFrameSearch()
            else:
                parseModeFromFile()
    currentFrame = StatisticsAPI.getCurrentFrame()
    movieLength = StatisticsAPI.getMovieLength()
    
    if script_mode == CREATING_POTENTIAL_DESYNCING_SAVE_STATES:
        if currentFrame in list_of_frames_to_search:
            saveStateForFrame(currentFrame)
        if currentFrame == movieLength - 2 and not os.path.isfile(getMemoryFilename(IS_ORIGINAL, IS_FRAME, currentFrame)):
            MemoryAPI.writeAllMemoryAsUnsignedBytesToFile(getMemoryFilename(IS_ORIGINAL, IS_FRAME, currentFrame))
        if currentFrame >= movieLength:
            writeNewFrameSearchToFile()
            dolphin.exitDolphin(CONTINUE_EXIT_CODE)
            
    if script_mode == TESTING_FOR_DESYNCING_SAVE_STATES:
        if currentFrame == movieLength - 2:
            MemoryAPI.writeAllMemoryAsUnsignedBytesToFile(getMemoryFilename(IS_NEW, IS_FRAME, currentFrame))
            if memoryFilesAreTheSame(getMemoryFilename(IS_ORIGINAL, IS_FRAME, currentFrame), getMemoryFilename(IS_NEW, IS_FRAME, currentFrame)):
                os.remove(getMemoryFilename(IS_NEW, IS_FRAME, currentFrame))
                os.remove(getSaveStateNameForFrame(frameToCheck))
                os.remove(getSaveStateNameForFrame(frameToCheck) + ".dtm")
                continueFile = open(baseDirectory + "continueFile.txt", "a")
                continueFile.write("CHECKED " + str(frameToCheck) + "\n")
                continueFile.flush()
                continueFile.close()
                dolphin.exitDolphin(CONTINUE_EXIT_CODE)
            else:
                #os.remove(getMemoryFilename(IS_NEW, IS_FRAME, currentFrame))
                #os.remove(getMemoryFilename(IS_ORIGINAL, IS_FRAME, currentFrame))
                frameDesyncingSaveStateWasMade = frameToCheck
                script_mode = SEARCHING_FOR_DESYNCING_FRAME_IN_ORIGINAL
                writeGeneralContinueFile()
                dolphin.exitDolphin(CONTINUE_EXIT_CODE)
                
    if script_mode == SEARCHING_FOR_DESYNCING_FRAME_IN_ORIGINAL:
        if currentFrame == frameToCheck:
            MemoryAPI.writeAllMemoryAsUnsignedBytesToFile(getMemoryFilename(IS_ORIGINAL, IS_FRAME, currentFrame))
            script_mode = SEARCHING_FOR_DESYNCING_FRAME_IN_NEW
            writeGeneralContinueFile()
            continueFile = open(baseDirectory + "continueFile.txt", "a")
            continueFile.write(">= " + str(beforeDesyncingFrame) + "\n")
            continueFile.write("< " + str(atOrAfterDesyncingFrame) + "\n")
            continueFile.write("TRY " + str(currentFrame) + "\n")
            continueFile.flush()
            continueFile.close()
            dolphin.exitDolphin(CONTINUE_EXIT_CODE)
            
    if script_mode == SEARCHING_FOR_DESYNCING_FRAME_IN_NEW:
        if currentFrame == frameToCheck:
            MemoryAPI.writeAllMemoryAsUnsignedBytesToFile(getMemoryFilename(IS_NEW, IS_FRAME, currentFrame))
            if memoryFilesAreTheSame(getMemoryFilename(IS_ORIGINAL, IS_FRAME, currentFrame), getMemoryFilename(IS_NEW, IS_FRAME, currentFrame)): 
                #means desync hasn't happened yet
                if currentFrame > beforeDesyncingFrame:
                    beforeDesyncingFrame = currentFrame
            else:
                #means desync has happened.
                if currentFrame < atOrAfterDesyncingFrame:
                    atOrAfterDesyncingFrame = currentFrame
            script_mode = SEARCHING_FOR_DESYNCING_FRAME_IN_ORIGINAL
            writeGeneralContinueFile()
            continueFile = open(baseDirectory + "continueFile.txt", "a")
            continueFile.write(">= " + str(beforeDesyncingFrame) + "\n")
            continueFile.write("< " + str(atOrAfterDesyncingFrame) + "\n")
            continueFile.flush()
            continueFile.close()
            dolphin.exitDolphin(CONTINUE_EXIT_CODE)
    if script_mode == COUNTING_TOTAL_INSTRUCTIONS:
        if currentFrame == desyncingFrameNum:
            OnInstructionHit.register(registers.getU32FromRegister("PC", 0), countTotalInstructionsFunction)
            return
            
    if script_mode == SEARCHING_FOR_DESYNCING_INSTRUCTION_IN_ORIGINAL:
        if currentFrame == desyncingFrameNum:
            OnInstructionHit.register(registers.getU32FromRegister("PC", 0), searchingForDesyncingInstructionInOriginalFunc)
            return
        
    if script_mode == SEARCHING_FOR_DESYNCING_INSTRUCTION_IN_NEW:
        if currentFrame == desyncingFrameNum:
            OnInstructionHit.register(registers.getU32FromRegister("PC", 0), searchingForDesyncingInstructionInNewFunc)
            return
            
    if script_mode == PRINTING_DESYNCING_INSTRUCTION:
        if currentFrame == desyncingFrameNum:
            OnInstructionHit.register(registers.getU32FromRegister("PC", 0), printingDesyncingInstructionFunc)
            return
        
            
print("Hello World!!!\n")
frameStartCallbackReference = OnFrameStart.register(mainFunction)