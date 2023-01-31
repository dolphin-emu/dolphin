add_library(dolphin-memory-engine
         Common/MemoryCommon.cpp
         Common/MemoryCommon.h
			Common/CommonTypes.h
         MemoryWatch/MemWatchEntry.cpp   
         MemoryWatch/MemWatchEntry.h   
         MemoryWatch/MemWatchTreeNode.cpp
         MemoryWatch/MemWatchTreeNode.h
         CheatEngineParser/CheatEngineParser.cpp
         CheatEngineParser/CheatEngineParser.h
         MemoryScanner/MemoryScanner.cpp
         MemoryScanner/MemoryScanner.h
			)
find_package(Qt5Core REQUIRED)

target_link_libraries(dolphin-memory-engine Qt5::Core)

