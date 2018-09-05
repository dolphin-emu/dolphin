In this directory, the build system may generate some files using the TableGen.cpp tool also provided in this directory.
This should be the case if you are building on MSVC.
CMake builds will usually generate them under Source/Core/Core/Generated/Core/PowerPC/Tables under the build directory.
Either way, they can be included with `#include "Core/PowerPC/Tables/filename.gen.extension"`

Generation can be done manually using the executable produced by TableGen.cpp with the following command-line arguments (assuming the executable is `./tablegen`):

* OpID_Ranges.gen.h: `./tablegen -i instructions.csv -ro OpID_Ranges.gen.h`
* OpID_DecodingTable.gen.cpp: `./tablegen -i instructions.csv -Do OpID_DecodingTable.gen.cpp`
* OpInfo.gen.cpp: `./tablegen -i instructions.csv -0p "\"" -s "\""  -2 -1p OpType:: -4d 1 -o OpInfo.gen.cpp`
* Interpreter_Table.gen.cpp: `./tablegen -i instructions.csv -3d '*' -p Interpreter:: -o Interpreter_Table.gen.cpp`

Different escaping might be required depending on your shell.
All files can be generated in a single run of TableGen by just concatenating the command-line arguments for the individual files (omitting the `-i instructions.csv` for all but one though).
