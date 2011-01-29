../../Binary/x64/DSPTool.exe -h dsp_code tests/mul_test.ds
mkdir emu
cp ../Core/Core/Src/DSP/Src/*.cpp emu
cp ../Core/Core/Src/DSP/Src/*.h emu
make

