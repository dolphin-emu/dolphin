../../Binary/x64/DSPTool.exe -h dsp_code tests/mul_test.ds
mkdir emu
cp ../Core/Core/DSP/*.cpp emu
cp ../Core/Core/DSP/*.h emu
make

