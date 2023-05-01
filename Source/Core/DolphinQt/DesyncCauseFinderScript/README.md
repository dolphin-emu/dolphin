To run the desync finder script, you first need to alter the values in properties.txt.

The first line of the file (which starts with "Game_Name") should contain the path to the game you want to run.
The second line of the file (which starts with "Movie_Name") should contain the path to the movie file of the game, which you want to introduce a desync on.
The third line of the file (which starts with "Early_Save_State_Movie_Name") should contain the path to a savestate for the movie specified above, which will act as the starting point to look for desyncs.

The desync finder script works as follows:

1. Play the movie + load the save state. Create random savestates while playing the movie back, and make a copy of RAM at the end of the movie.
2. Exit Dolphin, re-open it, and start playing the movie back from each of the generated savestates. Repeat this process until you find a savestate with different values of RAM at the end of the movie (indicating a desync).
3. Dolphin will then try playing the movie back from the early save state and playing back from the desyncing save state, to find the first frame when RAM is different between the 2.
4. Dolphin will then step though the desyncing frame in both the early save state and when playing back from the desyncing save state until it finds the desyncing instruction.
5. The program will then output the desyncing instruction, along with the instructions immediately before and after it.

Running the script:

python3 Main.py

IMPORTANT NOTE:

The script uses the contents of the continueFile.txt to figure out where to leave off if the user exits the script mid-way through and wants to come back to it later. As such, you should erase the contents of the file whenever you want to run a desync test with different properties. Alternatively, you can specify to delete the continueFile.txt and start a new test by passing in the command-line argument --FORCE_NEW

Usage:

python3 Main.py --FORCE_NEW