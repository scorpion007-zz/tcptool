@echo off
setlocal enabledelayedexpansion
set BIN_NAME=tcptool

echo Deploying %BIN_NAME%...

set DEPLOY_PATH=deploy
if exist %DEPLOY_PATH% (
    rd /q/s %DEPLOY_PATH%
    md %DEPLOY_PATH%
)

set FLAVORS=Debug RelWithDebInfo
for %%f in (%FLAVORS%) do (
	call :deploy_flavor %%f %DEPLOY_PATH%\%%f
)
goto :eof

REM Function to deploy a single flavor to a given output dir.
REM
:deploy_flavor

setlocal
set FLAV=%1

REM Outdir includes the build flavor (debug, retail)
REM
set OUTDIR=%2

echo Deploying %FLAV% to %OUTDIR%
echo --------------------------------------

md %OUTDIR%

REM Copy tcptool EXE/PDB.
REM
copy build\%FLAV%\%BIN_NAME%.exe %OUTDIR%\
copy build\%FLAV%\%BIN_NAME%.pdb %OUTDIR%\


endlocal
