# The Dolphin Lua API

Dolphin features an API that allows Lua scripts to interact with the emulator.
The Lua console can be opened by going to Tools -> Open Lua Console.
* The Browse... button allows you to browse your computer for a Lua script to be loaded into the console.
* The Run button will begin execution of the currently loaded Lua script.
* The Stop button will halt execution of the currently loaded Lua script, if it's still running.
* Console -> Clear will erase the contents of the output console.
* Help -> Lua Documentation will take you to the webpage for the free online book [Programming in Lua](https://www.lua.org/pil/contents.html)
which is a good way to start learning how to write Lua scripts.
* Help -> Dolphin Lua API will take you to this page.

## Emulator Functions

* void print(string str)
  * Predictably, this function simply writes str to the output console.

* void frameAdvance()
  * Advances Dolphin by one frame if paused, pauses otherwise. 
     This function will wait until Dolphin has advanced before returning

* int getFrameCount()
  * Returns Dolphin's internal frame count

* int getMovieLength()
  * Returns length of loaded movie. If no movie is loaded, this errors.

* void softReset()
  * Resets the emulator. Basically the same thing as pushing the reset button on a real GameCube/Wii.

* void saveState(int slot)
  * Saves a state in slot where slot can be any number from 0 to 99

* void loadState(int slot)
  * Loads the state at slot

* void setEmulatorSpeed(double percentage)
  * Sets Dolphins max speed to the percentage specified.

## Gamecube controller functions

* u8, u8 getAnalog()
  * Returns the x and y positions of Lua's virtual controller.

* void setAnalog(u8 xPos, u8 yPos)
  * Sets the analog stick to the cartesian ordered pair (xPos, yPos)

* void setAnalogPolar(int magnitude, int direction)
  * Sets the analog stick to the polar ordered pair (magnitude, direction)
  Note: Magnitude must be in the range [0, 128)

Dolphin represents the buttons on a GC controller as a 16-bit bitmask where:
* D-Pad left = 1
* D-Pad right = 2
* D-Pad down = 4
* D-Pad up = 8
* Z button = 16
* A button = 256
* B button = 512
* X button = 1024
* Y button = 2048
* Start button = 4096



* u16 getButtons()
  * Returns the bitmask described above.

* void setButtons(string buttons)
  * Takes a string and then checks each letter and sets the appropriate button
    * A = A button
    * B = B button
    * X = X button
    * Y = Y button
    * S = Start button
  * U = Unset all buttons

* u8, u8 getTriggers()
  * Returns the values of the left and right triggers, respectively

* void setTriggers(u8 left, u8 right)
  * Sets the left trigger to left and sets the right trigger to right
  