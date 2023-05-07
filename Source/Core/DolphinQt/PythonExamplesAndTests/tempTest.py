dolphin.importModule("StatisticsAPI", "1.0")

funcRef = -1

list_of_frames = []
def mainFunc():
    global list_of_frames
    list_of_frames.append(StatisticsAPI.getCurrentFrame())
    if StatisticsAPI.getCurrentFrame() >= 5000:
        outputFile = open("output.txt", "w")
        startOfBlock = -1
        endOfBlock = -1
        for entry in list_of_frames:
            if startOfBlock == -1:
                startOfBlock = entry
                endOfBlock = entry
                continue
            if endOfBlock + 1 == entry:
                endOfBlock = entry
            else:
                outputFile.write("Continguous block from " + str(startOfBlock)  + " to " + str(endOfBlock + 1) + "\n")
                startOfBlock = -1
                endOfBlock = -1
        if startOfBlock != -1:
            outputFile.write("Contiguous block from " + str(startOfBlock) + " to " + str(endOfBlock) + "\n")
        outputFile.flush()
        outputFile.close()
        OnFrameStart.unregister(funcRef)
             


funcRef = OnFrameStart.register(mainFunc)