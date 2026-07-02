@echo off
setlocal

rem Build firmware artifacts including HEX for the current firmware configuration.
rem Default target is F18 because that is what this workspace is using now.
set "TARGET_HW=18"
set "SCONS_ARGS="

:parse_args
if "%~1"=="" goto run_build
set "ARG=%~1"
if /I "%ARG:~0,10%"=="TARGET_HW=" (
    set "TARGET_HW=%ARG:~10%"
) else (
    set "SCONS_ARGS=%SCONS_ARGS% %~1"
)
shift
goto parse_args

:run_build
call "%~dp0fbt.cmd" TARGET_HW=%TARGET_HW% firmware_all %SCONS_ARGS%

endlocal
