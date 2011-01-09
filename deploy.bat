@echo off
setlocal
REM Simple batch script to upload the .smx file to a server via FTP. Note: Does _not_ 
REM upload configs or anything else.  This script is just for convenience when developing.

REM Instructions:

REM Just create a text file called ftp_credentials.cfg and add your ftp details with the following format:
REM host sphygmomanometer.ncftp.com
REM user myusername
REM pass mypassword 

REM Possible problems / extensions.
REM
REM 1. 	This assumes the root ftp directory is the same I'm using for the roto dev server.
REM 2.  It only supports a single server for the moment.
REM 
REM If you want to extend this just make another batch file that calls this one, then parameterise
REM the ftp credentials and the remote directory path.		

pushd %~dp0

set "PLUGIN_FILENAME=build\rotoblin.smx"

echo --------------------------------------------
echo Deploying plugin to server via FTP.
echo --------------------------------------------

REM I've made this come from one dir up just to stop anyone accidentally committing a file containing 
REM a password to the source repository.

"bin/ncftp/ncftpput.exe" -f ../ftp_credentials.cfg /l4d/left4dead/addons/sourcemod/plugins %PLUGIN_FILENAME%

echo --------------------------------------------
echo Finished.
echo --------------------------------------------

popd
endlocal