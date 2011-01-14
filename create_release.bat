@echo off
setlocal

REM batch script to package up the contents of rotoblin and create a release .zip 
REM that can be unzipped to install rotoblin.  
pushd %~dp0

REM temporary directory that will hold the release files prior to zipping
set "TEMP_DIR=release_temp" 

echo Cleaning directory: %TEMP_DIR%
if exist %TEMP_DIR% (
rmdir %TEMP_DIR% /S /Q
)
mkdir %TEMP_DIR%

REM Build the .smx file
echo Calling makefile.bat
call makefile.bat
echo Calling makefile_l4dready.bat
call makefile_l4dready.bat


REM Copy all of the required files (dependencies like left4downtown etc.) into a release folder
REM These are our simple dependencies; things like configs etc.
echo Creating release dir and copying files to correct locations
ROBOCOPY /E "src/cfg" "%TEMP_DIR%/cfg"
ROBOCOPY /E "src/maps" "%TEMP_DIR%/maps"

REM These are our other dependencies (plugins, configurations for plugins)
ROBOCOPY /E "rotoblin_dependencies" "%TEMP_DIR%"

REM Now we need to put the built rotoblin.smx in the correct place.  Note: copy is picky about slashes.
copy /Y "build\rotoblin.smx" "%TEMP_DIR%\addons\sourcemod\plugins\rotoblin.smx"
copy /Y "build\l4dready.smx" "%TEMP_DIR%\addons\sourcemod\plugins\l4dready.smx"

REM Finally, zip up the release folder using 7zip
echo zipping release dir

set "PACKAGED_ZIP=rotoblin_release.zip"
if exist %PACKAGED_ZIP% (
del /F /Q %PACKAGED_ZIP%
)

REM Note: changing the working directory here temporarily just to make the zipping easier.  If I 
REM fail to do this, the release zip ends up with the top level directory inside it being the same
REM as the %TEMP_DIR% value.
pushd %TEMP_DIR%
"../bin/7za920/7za.exe" a ../%PACKAGED_ZIP% .
popd

popd
endlocal