import dolphin
dolphin.importModule("StatisticsAPI", "1.0")
import StatisticsAPI

def isRecordingInput():
    return StatisticsAPI.isRecordingInput()

def isRecordingInputFromSaveState():
    return StatisticsAPI.isRecordingInputFromSaveState()

def isPlayingInput():
    return StatisticsAPI.isPlayingInput()
    
def isMovieActive():
    return StatisticsAPI.isMovieActive()
    
def getCurrentFrame():
    return StatisticsAPI.getCurrentFrame()
    
def getMovieLength():
    return StatisticsAPI.getMovieLength()
    
def getRerecordCount():
    return StatisticsAPI.getRerecordCount()
    
def getCurrentInputCount():
    return StatisticsAPI.getCurrentInputCount()
    
def getTotalInputCount():
    return StatisticsAPI.getTotalInputCount()
    
def getCurrentLagCount():
    return StatisticsAPI.getCurrentLagCount()
    
def getTotalLagCount():
    return StatisticsAPI.getTotalLagCount()

def getRAMSize():
    return StatisticsAPI.getRAMSize()

def getL1CacheSize():
    return StatisticsAPI.getL1CacheSize()

def getFakeVMemSize():
    return StatisticsAPI.getFakeVMemSize()

def getExRAMSize():
    return StatisticsAPI.getExRAMSize()

