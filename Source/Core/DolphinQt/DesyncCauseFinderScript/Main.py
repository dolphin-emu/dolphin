import subprocess
import os.path

pathToGame = ""
pathToMovieFile = ""
pathToEarlySaveState = ""


def mainLoop():
    global pathToGame
    global pathToMovieFile
    global pathToEarlySaveState
    returnResult = -1
    while True:
        subprocessReturnClass = subprocess.run(["C:/Users/Lobster/OneDrive/Desktop/lobsterZeldaDolphin/Binary/x64/Dolphin.exe",  pathToGame , "--movie", pathToMovieFile, "--save_state", pathToEarlySaveState])
        returnResult = subprocessReturnClass.returncode
        if returnResult == 0:
            print("Program aborted by the user. It'll continue where it left off if you don't delete the continueFile.txt file\n")
            return
        
        

def mainFunction():
    global pathToGame
    global pathToMovieFile
    global pathToEarlySaveState
    propertiesFile = open('properties.txt', 'r')
    lines = propertiesFile.readlines()
    
    for line in lines:
        pairOfValues = line.split(":", 1)
        if len(pairOfValues) < 2:
            continue
        key = pairOfValues[0].strip().lower()
        value = pairOfValues[1].strip()
        if key == "game_name":
            pathToGame = value
        elif key == "movie_name":
            pathToMovieFile = value
        elif key == "early_save_state_name" or key == "early_savestate_name":
            pathToEarlySaveState = value
        else:
            continue
            
    if pathToGame == "":
        print("The file properties.txt did not specify the path to the game iso. Include a line in the file with the format \"Game_Name:dir1/dir2/myGame.iso\"\n")
        return
    elif not os.path.isfile(pathToGame):
        print("Path to game iso of " + pathToGame + " did not represent a valid file. Format should be a file path like \"dir1/dir2/myGame.iso\"\n")
        return
    elif pathToMovieFile == "":
        print("The file properties.txt did not specify the path to the movie file name. Include a line in the file with the format \"Movie_Name:dir1/dir2/movieName.dtm\"\n")
        return
    elif not os.path.isfile(pathToMovieFile):
        print("Path to movie file of " + pathToMovieFile + " did not represent a valid file. Format should be a file path like \"dir1/dir2/movieName.dtm\"\n")
        return
    elif pathToEarlySaveState == "":
        print("The file properties.txt did not specify the path to the starting save state. Include a line in the file with the format \"Early_Save_State_Name:dir1/dir2/saveStateName.sav\"\n")
        return
    elif not os.path.isfile(pathToEarlySaveState):
        print("Path to starting save state of " + pathToEarlySaveState + " did not represent a valid file. Format should be a file path like \"dir1/dir2/saveStateName.sav\"\n")
        return
    else:
        mainLoop()

mainFunction()