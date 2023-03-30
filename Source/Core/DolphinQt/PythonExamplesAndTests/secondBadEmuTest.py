import testScript

dolphin.importModule("GameCubeControllerAPI", "1.0")
val = 0
def myFunc():
    global val
    GameCubeControllerAPI.setInputsForPoll( {"A": True, "Z": True })
    print(str(val))
    val = val + 1
OnGCControllerPolled.register(myFunc)