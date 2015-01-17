ECHO OFF
REM FF Prompt 1.1
REM Open a command prompt to run ffmpeg/ffplay/ffprobe
REM Copyright (C) 2013  Kyle Schwarz

TITLE FF Prompt

IF NOT EXIST bin\ffmpeg.exe (
  CLS
  ECHO bin\ffmpeg.exe could not be found.
  GOTO:error
)

CD bin || GOTO:error
PROMPT $G
CLS
ffmpeg -version
SET PATH=%CD%;%PATH%
ECHO.
ECHO For help run: ffmpeg -h
ECHO For formats run: ffmpeg -formats ^| more
ECHO For codecs run: ffmpeg -codecs ^| more
ECHO.
ECHO Current directory is now: "%CD%"
ECHO The bin directory has been added to PATH
ECHO.

CMD /F:ON /Q /K
GOTO:EOF

:error
  ECHO.
  ECHO Press any key to exit.
  PAUSE >nul
  GOTO:EOF
