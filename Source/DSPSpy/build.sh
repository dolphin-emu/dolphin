../../Binary/x64/DSPTool.exe -h dsp_code tests/mul_test.ds
mkdir emu
cp ../Core/DSPCore/Src/*.cpp emu
cp ../Core/DSPCore/Src/*.h emu
make

