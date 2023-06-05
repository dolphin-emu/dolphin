@echo off
if [%1] == [] goto NoFile
echo Given "%~1"
xdelta3-3.0.11-i686 -d -s "%~1" data.xdelta3 "Mario Party 4 (USA) (v1.01).iso"
pause
exit
:NoFile
echo Please drag a valid Mario Party 4 (USA) (v1.00) ISO onto this batch file.
pause
exit