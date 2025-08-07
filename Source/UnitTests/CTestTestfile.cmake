# CMake generated Testfile for 
# Source directory: /home/wells/dolphin/Source/UnitTests
# Build directory: /home/wells/dolphin/build/Source/UnitTests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(tests "/home/wells/dolphin/build/Binaries/Tests/tests")
set_tests_properties(tests PROPERTIES  _BACKTRACE_TRIPLES "/home/wells/dolphin/Source/UnitTests/CMakeLists.txt;10;add_test;/home/wells/dolphin/Source/UnitTests/CMakeLists.txt;0;")
subdirs("Common")
subdirs("Core")
subdirs("VideoCommon")
