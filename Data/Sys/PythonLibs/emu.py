import dolphin
dolphin.importModule("EmuAPI", "1.0")
import EmuAPI

def frameAdvance():
    EmuAPI.frameAdvance()
    
def loadState(stateName):
    EmuAPI.loadState(stateName)
    
def saveState(stateName):
    EmuAPI.saveState(stateName)
    
def playMovie(movieName):
    EmuAPI.playMovie(movieName)
    
def saveMovie(movieName):
    EmuAPI.saveMovie(movieName)
