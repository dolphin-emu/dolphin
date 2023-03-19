add_library(rcheevos
	RAInterface/RA_Consoles.h
	RAInterface/RA_Emulators.h
	RAInterface/RA_Interface.cpp
	RAInterface/RA_Interface.h
)

target_include_directories(RAInterface PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(RAInterface INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
)