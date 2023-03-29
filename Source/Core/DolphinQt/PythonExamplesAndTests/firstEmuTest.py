dolphin.importModule("EmuAPI", "1.0")
dolphin.importModule("MemoryAPI", "1.0")
EmuAPI.playMovie("testBaseRecording.dtm")
EmuAPI.loadState("testEarlySaveState.sav")

baseAddress = 0X80000123

inputTable = {baseAddress: 15, baseAddress + 1: 16, baseAddress + 2: 178, baseAddress + 3: -5}
MemoryAPI.write_bytes(inputTable)

resultsUnsigned = MemoryAPI.read_unsigned_bytes(baseAddress, 4)

for intKey, intValue in resultsUnsigned.items():
    print("Table Address: " + str(intKey) + ", Value: " + str(intValue))
print("Done!")

resultsSigned = MemoryAPI.read_signed_bytes(baseAddress, 4)

for intKey, intValue in resultsSigned.items():
    print("Table Address: " + str(intKey) + ", Value: " + str(intValue))
print("Done!")