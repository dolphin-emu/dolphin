@echo off
if [%1] == [] goto NoFile
echo Given "%~1"
xdelta3-3.0.11-i686 -d -s "%~1" data.xdelta3 "Mario Party 7 (USA) [Widescreen].iso"
pause
exit
:NoFile
echo Please drag a valid Mario Party 7 (USA) (v1.02) ISO onto this batch file.
pause
exit