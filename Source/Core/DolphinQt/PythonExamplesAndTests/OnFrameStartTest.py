dolphin.importModule("EmuAPI", "1.0")
dolphin.importModule("GameCubeControllerAPI", "1.0")
EmuAPI.playMovie("testBaseRecording.dtm")
EmuAPI.loadState("testEarlySaveState.sav")

def testFunction():
    GameCubeControllerAPI.setInputsForPoll( { "A": True, "B": True, "X": False } )
    return
    
OnGCControllerPolled.register(testFunction)
