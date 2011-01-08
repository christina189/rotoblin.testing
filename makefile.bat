@echo off
setlocal

set "PLUGIN_DIR=src\scripting"
set "PLUGIN_NAME=rotoblin.main"
set "RELEASE_NAME=rotoblin"
set "SPCOMP=bin\sourcepawn\spcomp.exe"

echo --------------------------------------------
echo Building %PLUGIN_NAME%.sp ...
echo --------------------------------------------

"%SPCOMP%" -D%PLUGIN_DIR% "%PLUGIN_NAME%.sp"
move /Y "%PLUGIN_DIR%\%PLUGIN_NAME%.smx" "%RELEASE_NAME%.smx" 

echo --------------------------------------------
echo Built %PLUGIN_NAME%.sp as %RELEASE_NAME%.smx
echo --------------------------------------------

endlocal
pause
@echo on