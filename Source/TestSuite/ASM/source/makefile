all:
	g++ -S -arch ppc Instructions/asm_float.cpp
	g++ -g -arch ppc dolphintest_asm.cpp asm_tables.cpp Helpers.cpp Instructions/asm_integer.cpp Instructions/asm_float.cpp OSX/Init.cpp -o Test