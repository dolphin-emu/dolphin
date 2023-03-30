import emu
print("Hello World!")
emu.playMovie("testBaseRecording.dtm")
print("At next line...")
emu.loadState("testEarlySaveState.sav")
emu.saveState("altState.sav")
emu.saveMovie("altMovie.dtm")
emu.frameAdvance()