@echo off

set originalISO=%~1




	:: First, ensure a file was provided to the script.

if NOT "%originalISO%"=="" goto :sourceProvided

echo.
echo  To use this, drag-and-drop your Vanilla Mario Party 8 (US) ISO or WBFS onto the batch file.
echo.
echo  (That is, the actual file icon in the folder, not this window.)
echo. 
echo  Press any key to exit. . . && pause > nul

goto eof


:sourceProvided

echo.
echo  Constructing the Mario Party 8 GC ControllerWBFS Please stand by....

cd /d %~dp0
cd tools

wit extract "%originalISO%" --dest=temp
IF exist "temp/DATA" (xcopy "../mp8motion" "temp/DATA" /s /y /e) ELSE (xcopy "../mp8motion" "temp" /s /y /e)
wit copy "temp" "../Mario Party 8 GC Controller.wbfs"
rmdir /s /q temp

echo. && echo.
echo        Construction complete!
echo.
echo  Press any key to exit. . . && pause > nul
:eof