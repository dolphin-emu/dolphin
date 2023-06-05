@echo off
if [%1] == [] goto NoFile
echo Given "%~1"
xdelta3-3.0.11-i686 -d -s "%~1" data.xdelta3 "Mario Party 5 (USA) [Widescreen].iso"
pause
exit
:NoFile
echo Please drag a valid Mario Party 5 (USA) ISO onto this batch file.
pause
exit